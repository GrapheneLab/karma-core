/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <graphene/chain/credit_evaluator.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/credit_object.hpp>
#include <graphene/chain/exchange_rate_object.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <graphene/chain/exchange_rate_object.hpp>

#include <iostream>
#include <string>

namespace graphene { namespace chain {
//***************************************************************************************************************************************//
//***************************************************************************************************************************************//
//***************************************************************************************************************************************//

void_result credit_request_evaluator::do_evaluate( const credit_request_operation& op )
{ 
      
   try {

      database& d = db( );
      const account_object& borrower_account = op.borrower( d );
      const asset_object& deposit_asset_type = op.deposit_asset.asset_id( d );  

      const asset_object& deposit = op.deposit_asset.asset_id( d );      
      const asset_object& loan = op.loan_asset.asset_id( d );

      double deposit_in_core = 0;

      if( deposit.amount_to_real( op.deposit_asset.amount ) < 0.0 )
            FC_ASSERT( 0, "deposit can't be < 0" ); 

      if( loan.amount_to_real( op.loan_asset.amount ) < 0.0 )
            FC_ASSERT( 0, "loan can't be < 0" );             

      if( deposit.amount_to_real( op.deposit_asset.amount ) != 0.0 )
      {
            double deposit_exchange_rate = get_exchange_rate_by_symbol(&d, deposit.symbol);
            deposit_in_core = deposit.amount_to_real( op.deposit_asset.amount ) * deposit_exchange_rate;
      }

      double loan_exchange_rate = get_exchange_rate_by_symbol(&d, loan.symbol);
      double loan_in_core = loan.amount_to_real( op.loan_asset.amount ) * loan_exchange_rate;  

      if( op.loan_period == 0 )
            FC_ASSERT( 0, "Loan period can't be 0" ); 

      if( op.loan_persent == 0.0 )
            FC_ASSERT( 0, "Loan persent can't be 0" ); 

      if( deposit.amount_to_real( op.deposit_asset.amount ) != 0.0 )
      {            
            if( deposit_in_core < ( ( loan_in_core / 100 ) * DEPOSIT_PERSENT ) )
                  FC_ASSERT( 0, "Deposit less % of credit" );    
            try {
                  bool insufficient_balance = d.get_balance( borrower_account, deposit_asset_type ).amount >= op.deposit_asset.amount;
                  FC_ASSERT( insufficient_balance,
                        "Insufficient Balance: ${balance}, unable to deposit '${deposit}' from account '${a}' to '${t}'", 
                        ( "a",borrower_account.name )( "deposit", d.to_pretty_string( op.deposit_asset ) )( "balance", d.to_pretty_string( d.get_balance( borrower_account, deposit_asset_type ) ) ) );

                  return void_result( );
            } FC_RETHROW_EXCEPTIONS( error, "Unable to deposit ${a}", ( "a",d.to_pretty_string( op.loan_asset ) ) );
      }

      if( db().head_block_time() >= HARDFORK_CORE_KARMA_2_TIME )
      {
            const auto& idx = db().get_index_type<account_index>().indices().get<by_name>();
            std::string special_account_name = db().get_global_properties().parameters.get_bonus_options().special_account_name;
            auto itr_account_obj = idx.find(special_account_name);
            FC_ASSERT( itr_account_obj != idx.end(), "No special account found: ${a}.",("a", special_account_name));
      }

      return void_result();      
}  FC_CAPTURE_AND_RETHROW( ( op ) ) }

object_id_type credit_request_evaluator::do_apply( const credit_request_operation& o )
{ 
   try {  
          
      const auto& new_credit_object = db( ).create<credit_object>( [&]( credit_object& obj )
      {
         const database& d = db( );
         const asset_object& deposit = o.deposit_asset.asset_id( d );   

         if( deposit.amount_to_real( o.deposit_asset.amount ) != 0.0 )
            obj.borrower.collateral_free = false;
         else
            obj.borrower.collateral_free = true;

         obj.borrower.borrower = o.borrower;
         obj.borrower.loan_asset = o.loan_asset;
         obj.borrower.loan_period = o.loan_period;
         obj.borrower.loan_persent = ( double )o.loan_persent;
         obj.borrower.loan_memo = o.loan_memo;
         // move money to deposit asset
         obj.borrower.deposit_asset = o.deposit_asset;

         obj.request_creation_time = db( ).head_block_time( );
         obj.status = e_credit_object_status::wating_for_acceptance;
                  
         std::string time_str = boost::posix_time::to_iso_string(boost::posix_time::from_time_t( obj.request_creation_time.sec_since_epoch( ) ));
         std::string id = fc::to_string(obj.id.space()) + "." + fc::to_string(obj.id.type()) + "." + fc::to_string(obj.id.instance());

         boost::uuids::string_generator gen;
         boost::uuids::uuid u1 = gen( id + time_str + time_str );
         obj.object_uuid = to_string(u1);
         
         const account_object& borrower_account = o.borrower( db( ) );
         std::stringstream ss;
         std::string time_json = db( ).head_block_time( ).to_iso_string( );
         ss << time_json << " : " << "was created with id = " << obj.object_uuid << " by - " << borrower_account.name;
         obj.history.push_back( ss.str( ) );

         boost::property_tree::ptree root, details;
         std::stringstream ss_json;
         
         if(obj.history_json.size())
         {
            ss_json << obj.history_json;
            boost::property_tree::read_json(ss_json, root);
            ss_json.str( std::string() );
            ss_json.clear();
         }   
         details.put("type","request_creation");
         details.put("id",obj.object_uuid);
         details.put("borrower",borrower_account.name);
         root.add_child(time_json, details);
         
         boost::property_tree::write_json(ss_json, root);
         obj.history_json = ss_json.str();
         obj.history_json.erase(std::remove(obj.history_json.begin(), obj.history_json.end(), '\n'), obj.history_json.end());
         obj.history_json.erase(std::remove(obj.history_json.begin(), obj.history_json.end(), ' '), obj.history_json.end());
      });
      // deposit money         
      db( ).adjust_balance( o.borrower, -o.deposit_asset );
   return new_credit_object.id;
} FC_CAPTURE_AND_RETHROW( ( o ) ) }

//***************************************************************************************************************************************//
//***************************************************************************************************************************************//
//***************************************************************************************************************************************//

void_result credit_approve_evaluator::do_evaluate( const credit_approve_operation& op )
{ 
   return void_result( );
}

object_id_type credit_approve_evaluator::do_apply( const credit_approve_operation& o )
{ 
   try {          
   object_id_type ret;
   bool credit_request_founded = false;
   auto& e = db( ).get_index_type<credit_index>( ).indices( ).get<by_uuid>( );

   for( auto itr = e.begin( ); itr != e.end( ); itr++ )
   {       
      if( ( *itr ).object_uuid.compare( o.credit_request_uuid ) == 0 )
      {
            if( ( *itr ).status != e_credit_object_status::wating_for_acceptance )
                  FC_ASSERT( 0, "credit request allready accepted!" );

            const account_object& creditor_account = o.creditor( db( ) );
            const asset_object& loan_asset_type = ( *itr ).borrower.loan_asset.asset_id( db( ) );  

            bool insufficient_balance = db( ).get_balance( creditor_account, loan_asset_type ).amount >= ( *itr ).borrower.loan_asset.amount;
            FC_ASSERT( insufficient_balance, "Insufficient Balance" );
   
            db( ).modify( *itr, [this,&o,&ret]( credit_object& b ) 
            {
                  b.creditor.creditor = o.creditor;
                  b.creditor.credit_memo = o.credit_memo;
            
                  const asset_object& loan_asset_type = b.borrower.loan_asset.asset_id( db( ) );    
                  b.creditor.monthly_payment = b.calculate_monthly_payment( loan_asset_type.amount_to_real( b.borrower.loan_asset.amount ), ( double )b.borrower.loan_persent, b.borrower.loan_period );

                  b.request_approvation_time = db( ).head_block_time( );
                  b.settle_month_elapsed = 0;
                  b.status = e_credit_object_status::in_progress;

                  const account_object& creditor_account = o.creditor( db( ) );
                  std::stringstream ss;
                  std::string time_json = db( ).head_block_time( ).to_iso_string( );
                  std::string monthly_payment = std::to_string(b.creditor.monthly_payment);

                  ss << time_json << " : " << "credit with id = " << b.object_uuid << " was accepted by - " << creditor_account.name << 
                  " loan_amount = " << loan_asset_type.amount_to_string(b.borrower.loan_asset.amount) <<
                  " loan_period_in_month = " << b.borrower.loan_period << " monthly_payment = " << monthly_payment ;
                  b.history.push_back( ss.str( ) );

                  boost::property_tree::ptree root, details;
                  std::stringstream ss_json;

                  if(b.history_json.size())
                  {
                        ss_json << b.history_json;
                        boost::property_tree::read_json(ss_json, root);      
                        ss_json.str( std::string() );
                        ss_json.clear();
                  }

                  details.put("type","request_approved");
                  details.put("id",b.object_uuid);
                  details.put("creditor",creditor_account.name);
                  details.put("loan_amount",loan_asset_type.amount_to_string(b.borrower.loan_asset.amount));
                  details.put("loan_period_in_month",b.borrower.loan_period);
                  details.put("monthly_payment",monthly_payment);
                  root.add_child(time_json, details);
                  
                  boost::property_tree::write_json(ss_json, root);
                  b.history_json = ss_json.str();
                  b.history_json.erase(std::remove(b.history_json.begin(), b.history_json.end(), '\n'), b.history_json.end());
                  b.history_json.erase(std::remove(b.history_json.begin(), b.history_json.end(), ' '), b.history_json.end());

                  ret = b.id;            
            });

            db( ).adjust_balance( ( *itr ).creditor.creditor, -( *itr ).borrower.loan_asset );

            // pay %bonus from credit sum to special karma account and referals
            if( db().head_block_time() >= HARDFORK_CORE_KARMA_2_TIME )
            {
                  chain_parameters::ext::credit_referrer_bonus_options bo = db().get_global_properties().parameters.get_bonus_options();

                  auto idx = db().get_index_type<account_index>().indices().get<by_name>().find(bo.special_account_name);
                  account_id_type special_account_id = (*idx).get_id();

                  double creditor_percent = (db().head_block_time() < ( *itr ).creditor.creditor(db()).credit_referrer_expiration_date) ? bo.creditor_referrer_bonus : 0;
                  double borrower_percent = (db().head_block_time() < ( *itr ).borrower.borrower(db()).credit_referrer_expiration_date) ? bo.borrower_referrer_bonus : 0;
                  double special_account_percent = bo.karma_account_bonus - borrower_percent - creditor_percent;

                  asset asset_sum_for_creditor(( *itr ).borrower.loan_asset.amount.value * creditor_percent/100, loan_asset_type.get_id());
                  asset asset_sum_for_borrower(( *itr ).borrower.loan_asset.amount.value * borrower_percent/100, loan_asset_type.get_id());
                  asset asset_sum_to_special_account(( *itr ).borrower.loan_asset.amount.value * special_account_percent/100, loan_asset_type.get_id());

                  db( ).adjust_balance( ( *itr ).creditor.creditor(db()).credit_referrer, asset_sum_for_creditor);
                  db( ).adjust_balance( ( *itr ).borrower.borrower(db()).credit_referrer, asset_sum_for_borrower);   
                  db( ).adjust_balance( special_account_id, asset_sum_to_special_account);   

                  db( ).adjust_balance( ( *itr ).borrower.borrower, ( *itr ).borrower.loan_asset 
                                                - asset_sum_for_creditor - asset_sum_for_borrower - asset_sum_to_special_account);   
            }
            else
                db( ).adjust_balance( ( *itr ).borrower.borrower, ( *itr ).borrower.loan_asset );   
                     
            credit_request_founded = true;
            break;
      }
   }
   FC_ASSERT( credit_request_founded, "credit request not found!" );
   return ret;
} FC_CAPTURE_AND_RETHROW( ( o ) ) }

//***************************************************************************************************************************************//
//***************************************************************************************************************************************//
//***************************************************************************************************************************************//

void_result credit_request_cancel_evaluator::do_evaluate( const credit_request_cancel_operation& op )
{ 
   return void_result( );
}

object_id_type credit_request_cancel_evaluator::do_apply( const credit_request_cancel_operation& o )
{ 
   try {          
   object_id_type ret;
   bool credit_request_founded = false;
   auto& e = db( ).get_index_type<credit_index>( ).indices( ).get<by_uuid>( );

   for( auto itr = e.begin( ); itr != e.end( ); itr++ )
   {       
      if( ( *itr ).object_uuid.compare( o.credit_request_uuid ) == 0 )
      {
            if(o.borrower != ( *itr ).borrower.borrower)
                  FC_ASSERT( 0, "Only owner can cancel the credit request!" );
            
            if( ( *itr ).status != e_credit_object_status::wating_for_acceptance )
                  FC_ASSERT( 0, "credit request allready accepted!" );

            db( ).adjust_balance( ( *itr ).borrower.borrower, ( *itr ).borrower.deposit_asset );
            db( ).remove( *itr );
            credit_request_founded = true;
            break;
      }
   }
   FC_ASSERT( credit_request_founded, "credit request not found!" );
   return ret;
} FC_CAPTURE_AND_RETHROW( ( o ) ) }

//***************************************************************************************************************************************//
//***************************************************************************************************************************************//
//***************************************************************************************************************************************//

void_result comment_credit_request_evaluator::do_evaluate( const comment_credit_request_operation& op )
{ 
   return void_result( );
}

object_id_type comment_credit_request_evaluator::do_apply( const comment_credit_request_operation& o )
{ 
   try {          
   object_id_type ret;
   bool credit_request_founded = false;
   auto& e = db( ).get_index_type<credit_index>( ).indices( ).get<by_uuid>( );

   for( auto itr = e.begin( ); itr != e.end( ); itr++ )
   {       
      if( ( *itr ).object_uuid.compare( o.credit_request_uuid ) == 0 )
      {
            if( ( *itr ).status != e_credit_object_status::wating_for_acceptance )
                  FC_ASSERT( 0, "credit request allready accepted!" );

            db( ).modify( *itr, [this,&o,&ret]( credit_object& b ) 
            {
                  b.comments[o.credit_memo] = o.creditor;
            });
            credit_request_founded = true;
            break;
      }
   }
   FC_ASSERT( credit_request_founded, "credit request not found!" );
   return ret;
} FC_CAPTURE_AND_RETHROW( ( o ) ) }

//***************************************************************************************************************************************//
//***************************************************************************************************************************************//
//***************************************************************************************************************************************//

void_result settle_credit_operation_evaluator::do_evaluate( const settle_credit_operation& op )
{ 
   return void_result( );
}

object_id_type settle_credit_operation_evaluator::do_apply( const settle_credit_operation& o )
{ 
   try {          
   object_id_type ret;
   bool credit_request_founded = false;
   auto& e = db( ).get_index_type<credit_index>( ).indices( ).get<by_uuid>( );

   database& d = db( );  
   for( auto itr = e.begin( ); itr != e.end( ); itr++ )
   {       
      if( ( *itr ).object_uuid.compare( o.credit_request_uuid ) == 0 )
      {
            const asset_object& loan = ( *itr ).borrower.loan_asset.asset_id( d );
            const account_object& borrower_account = o.borrower( db( ) );
            double settle_sum = 0.0;  

            if( ( *itr ).status != e_credit_object_status::in_progress )
                  FC_ASSERT( 0, "credit request don't accepted!" );

            if(o.borrower != ( *itr ).borrower.borrower)
                  FC_ASSERT( 0, "Only owner can settle the credit operation!" );

            if( ( *itr ).settle_month_elapsed == 0 )
                  settle_sum = ( *itr ).borrower.loan_persent / 12.0 + loan.amount_to_real( ( *itr ).borrower.loan_asset.amount );
            else
                  settle_sum = ( *itr ).creditor.monthly_payment * ( ( *itr ).borrower.loan_period - ( *itr ).settle_month_elapsed );   
            
            asset settle( settle_sum * pow( 10, loan.precision), loan.get_id( ) );

            bool insufficient_balance = db( ).get_balance( borrower_account, loan ).amount >= settle.amount;
            FC_ASSERT( insufficient_balance, "Insufficient Balance" );  

            db( ).adjust_balance( ( *itr ).borrower.borrower, -settle );
            db( ).adjust_balance( ( *itr ).creditor.creditor, settle );                   

            db( ).modify( *itr, [this,&o,&ret,&d]( credit_object& b ) 
            {
                  const asset_object& deposit = b.borrower.deposit_asset.asset_id( d );
                  db( ).adjust_balance( b.borrower.borrower, b.borrower.deposit_asset );
        
                  update_account_karma(&db(), b.borrower.borrower, KARMA_BONUS_FOR_CREDIT_PAYMENT, "Credit complete forced.");

                  std::stringstream ss;
                  std::string time_json = db( ).head_block_time( ).to_iso_string( );
                  ss << time_json << " : " << "Credit complete forced, " << deposit.amount_to_pretty_string( b.borrower.deposit_asset ) << " returned.";
                  b.history.push_back( ss.str( ) );

                  boost::property_tree::ptree root, details;
                  std::stringstream ss_json;

                  if(b.history_json.size())
                  {
                        ss_json << b.history_json;
                        boost::property_tree::read_json(ss_json, root);      
                        ss_json.str( std::string() );
                        ss_json.clear();
                  }      
                  
                  details.put("type","credit_complete_forced");
                  details.put("status","deposit_returned");
                  details.put("asset",deposit.symbol);
                  details.put("sum",deposit.amount_to_string( b.borrower.deposit_asset.amount));
                  root.add_child(time_json, details);

                  boost::property_tree::write_json(ss_json, root);
                  b.history_json = ss_json.str();
                  b.history_json.erase(std::remove(b.history_json.begin(), b.history_json.end(), '\n'), b.history_json.end());
                  b.history_json.erase(std::remove(b.history_json.begin(), b.history_json.end(), ' '), b.history_json.end());

                  b.borrower.deposit_asset = asset( 0, deposit.get_id( ) );
                  b.status = e_credit_object_status::complete_normal;
            });

            credit_request_founded = true;
            break;
      }
   }
   FC_ASSERT( credit_request_founded, "credit request not found!" );
   return ret;
} FC_CAPTURE_AND_RETHROW( ( o ) ) }

//***************************************************************************************************************************************//
//***************************************************************************************************************************************//
//***************************************************************************************************************************************//
} } // graphene::chain
