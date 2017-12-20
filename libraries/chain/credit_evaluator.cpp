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
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

#include <iostream>
#include <string>

namespace graphene { namespace chain {
//***************************************************************************************************************************************//
//***************************************************************************************************************************************//
//***************************************************************************************************************************************//

void_result credit_request_evaluator::do_evaluate( const credit_request_operation& op )
{ 
      
   try {

      const database& d = db( );
      const account_object& borrower_account = op.borrower( d );
      const asset_object& deposit_asset_type = op.deposit_asset.asset_id( d );  

      const asset_object& deposit = op.deposit_asset.asset_id( d );      
      const asset_object& loan = op.loan_asset.asset_id( d );

      if( deposit.amount_to_real( op.deposit_asset.amount ) < 0.0 )
            FC_ASSERT( 0, "deposit can't be < 0" ); 

      if( loan.amount_to_real( op.loan_asset.amount ) < 0.0 )
            FC_ASSERT( 0, "loan can't be < 0" );             

      double deposit_in_core = deposit.amount_to_real( op.deposit_asset.amount ) * deposit.options.core_exchange_rate.to_real( );
      double loan_in_core = loan.amount_to_real( op.loan_asset.amount ) * loan.options.core_exchange_rate.to_real( );  

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
         obj.object_uuid = to_string( boost::uuids::random_generator( )( ) );

         const account_object& borrower_account = o.borrower( db( ) );
         std::stringstream ss;
         ss << db( ).head_block_time( ).to_iso_string( ) << " : " << "was created with id = " << obj.object_uuid << " by - " << borrower_account.name;
         obj.history.push_back( ss.str( ) );
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
                  ss << db( ).head_block_time( ).to_iso_string( ) << " : " << "credit with id = " << b.object_uuid << " was accepted by - " << creditor_account.name << " loan_amount = " << b.borrower.loan_asset.amount.value / 100000 <<
                  " loan_period_in_month = " << b.borrower.loan_period << " monthly_payment = " << b.creditor.monthly_payment ;
                  b.history.push_back( ss.str( ) );

                  ret = b.id;            
            });
            // transfer money to borrower
            db( ).adjust_balance( ( *itr ).creditor.creditor, -( *itr ).borrower.loan_asset );
            db( ).adjust_balance( ( *itr ).borrower.borrower, ( *itr ).borrower.loan_asset );   
                     
            credit_request_founded = true;
            break;
      }
   }
   FC_ASSERT( credit_request_founded, "credit request not founed!" );
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
            if( ( *itr ).status != e_credit_object_status::wating_for_acceptance )
                  FC_ASSERT( 0, "credit request allready accepted!" );

            db( ).adjust_balance( ( *itr ).borrower.borrower, ( *itr ).borrower.deposit_asset );
            db( ).remove( *itr );
            credit_request_founded = true;
            break;
      }
   }
   FC_ASSERT( credit_request_founded, "credit request not founed!" );
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
   FC_ASSERT( credit_request_founded, "credit request not founed!" );
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
         
            if( ( *itr ).settle_month_elapsed == 0 )
                  settle_sum = ( *itr ).borrower.loan_persent / 12.0 + loan.amount_to_real( ( *itr ).borrower.loan_asset.amount );
            else
                  settle_sum = ( *itr ).creditor.monthly_payment * ( ( *itr ).borrower.loan_period - ( *itr ).settle_month_elapsed );   
            
            asset settle( settle_sum * 100000, loan.get_id( ) ); 
            bool insufficient_balance = db( ).get_balance( borrower_account, loan ).amount >= settle.amount;
            FC_ASSERT( insufficient_balance, "Insufficient Balance" );  

            db( ).adjust_balance( ( *itr ).borrower.borrower, -settle );
            db( ).adjust_balance( ( *itr ).creditor.creditor, settle );                   

            db( ).modify( *itr, [this,&o,&ret,&d]( credit_object& b ) 
            {
                  const asset_object& deposit = b.borrower.deposit_asset.asset_id( d );
                  db( ).adjust_balance( b.borrower.borrower, b.borrower.deposit_asset );
        
                  std::stringstream ss;
                  ss << db( ).head_block_time( ).to_iso_string( ) << " : " << "Credit complete forced, " << deposit.amount_to_pretty_string( b.borrower.deposit_asset ) << " returned.";
                  b.history.push_back( ss.str( ) );

                  b.borrower.deposit_asset = asset( 0, deposit.get_id( ) );
                  b.status = e_credit_object_status::complete_normal;
            });

            credit_request_founded = true;
            break;
      }
   }
   FC_ASSERT( credit_request_founded, "credit request not founed!" );
   return ret;
} FC_CAPTURE_AND_RETHROW( ( o ) ) }

//***************************************************************************************************************************************//
//***************************************************************************************************************************************//
//***************************************************************************************************************************************//
} } // graphene::chain
