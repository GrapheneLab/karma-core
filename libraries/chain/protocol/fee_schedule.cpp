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
#include <algorithm>
#include <graphene/chain/protocol/fee_schedule.hpp>
#include <fc/smart_ref_impl.hpp>
#include <graphene/chain/hardfork.hpp>

namespace fc
{
   // explicitly instantiate the smart_ref, gcc fails to instantiate it in some release builds
   //template graphene::chain::fee_schedule& smart_ref<graphene::chain::fee_schedule>::operator=(smart_ref<graphene::chain::fee_schedule>&&);
   //template graphene::chain::fee_schedule& smart_ref<graphene::chain::fee_schedule>::operator=(U&&);
   //template graphene::chain::fee_schedule& smart_ref<graphene::chain::fee_schedule>::operator=(const smart_ref&);
   //template smart_ref<graphene::chain::fee_schedule>::smart_ref();
   //template const graphene::chain::fee_schedule& smart_ref<graphene::chain::fee_schedule>::operator*() const;
}

#define MAX_FEE_STABILIZATION_ITERATION 4

namespace graphene { namespace chain {

   typedef fc::smart_ref<fee_schedule> smart_fee_schedule;

   static smart_fee_schedule tmp;

   fee_schedule::fee_schedule()
   {
   }

   fee_schedule fee_schedule::get_default()
   {
      fee_schedule result;
      for( int i = 0; i < fee_parameters().count(); ++i )
      {
         fee_parameters x; x.set_which(i);
         result.parameters.insert(x);
      }
      return result;
   }

   struct fee_schedule_validate_visitor
   {
      typedef void result_type;

      template<typename T>
      void operator()( const T& p )const
      {
         //p.validate();
      }
   };

   void fee_schedule::validate()const
   {
      for( const auto& f : parameters )
         f.visit( fee_schedule_validate_visitor() );
   }

   struct calc_fee_visitor
   {
      typedef uint64_t result_type;

      const fee_schedule& param;
      const int current_op;
      calc_fee_visitor( const fee_schedule& p, const operation& op ):param(p),current_op(op.which()){}

      template<typename OpType>
      result_type operator()( const OpType& op )const
      {
         try {
            return op.calculate_fee( param.get<OpType>() ).value;
         } catch (fc::assert_exception e) {
             fee_parameters params; params.set_which(current_op);
             auto itr = param.parameters.find(params);
             if( itr != param.parameters.end() ) params = *itr;
             return op.calculate_fee( params.get<typename OpType::fee_parameters_type>() ).value;
         }
      }
   };

   struct set_fee_visitor
   {
      typedef void result_type;
      asset _fee;

      set_fee_visitor( asset f ):_fee(f){}

      template<typename OpType>
      void operator()( OpType& op )const
      {
         op.fee = _fee;
      }
   };

   struct zero_fee_visitor
   {
      typedef void result_type;

      template<typename ParamType>
      result_type operator()(  ParamType& op )const
      {
         memset( (char*)&op, 0, sizeof(op) );
      }
   };

   void fee_schedule::zero_all_fees()
   {
      *this = get_default();
      for( fee_parameters& i : parameters )
         i.visit( zero_fee_visitor() );
      this->scale = 0;
   }

   asset fee_schedule::calculate_fee( const operation& op, const price& core_exchange_rate )const
   {
      auto base_value = op.visit( calc_fee_visitor( *this, op ) );
      auto scaled = fc::uint128(base_value) * scale;
      scaled /= GRAPHENE_100_PERCENT;
      FC_ASSERT( scaled <= GRAPHENE_MAX_SHARE_SUPPLY );
      //idump( (base_value)(scaled)(core_exchange_rate) );
      auto result = asset( scaled.to_uint64(), asset_id_type(0) ) * core_exchange_rate;
      //FC_ASSERT( result * core_exchange_rate >= asset( scaled.to_uint64()) );

      while( result * core_exchange_rate < asset( scaled.to_uint64()) )
        result.amount++;

      FC_ASSERT( result.amount <= GRAPHENE_MAX_SHARE_SUPPLY );
      return result;
   }

   asset fee_schedule::set_fee( operation& op, const price& core_exchange_rate )const
   {
      auto f = calculate_fee( op, core_exchange_rate );
      auto f_max = f;
      for( int i=0; i<MAX_FEE_STABILIZATION_ITERATION; i++ )
      {
         op.visit( set_fee_visitor( f_max ) );
         auto f2 = calculate_fee( op, core_exchange_rate );
         if( f == f2 )
            break;
         f_max = std::max( f_max, f2 );
         f = f2;
         if( i == 0 )
         {
            // no need for warnings on later iterations
            wlog( "set_fee requires multiple iterations to stabilize with core_exchange_rate ${p} on operation ${op}",
               ("p", core_exchange_rate) ("op", op) );
         }
      }
      return f_max;
   }

   void chain_parameters::validate()const
   {
      current_fees->validate();
      FC_ASSERT( reserve_percent_of_fee <= GRAPHENE_100_PERCENT );
      FC_ASSERT( network_percent_of_fee <= GRAPHENE_100_PERCENT );
      FC_ASSERT( lifetime_referrer_percent_of_fee <= GRAPHENE_100_PERCENT );
      FC_ASSERT( network_percent_of_fee + lifetime_referrer_percent_of_fee <= GRAPHENE_100_PERCENT );

      FC_ASSERT( block_interval >= GRAPHENE_MIN_BLOCK_INTERVAL );
      FC_ASSERT( block_interval <= GRAPHENE_MAX_BLOCK_INTERVAL );
      FC_ASSERT( block_interval > 0 );
      FC_ASSERT( maintenance_interval > block_interval,
                 "Maintenance interval must be longer than block interval" );
      FC_ASSERT( maintenance_interval % block_interval == 0,
                 "Maintenance interval must be a multiple of block interval" );
      FC_ASSERT( maximum_transaction_size >= GRAPHENE_MIN_TRANSACTION_SIZE_LIMIT,
                 "Transaction size limit is too low" );
      FC_ASSERT( maximum_block_size >= GRAPHENE_MIN_BLOCK_SIZE_LIMIT,
                 "Block size limit is too low" );
      FC_ASSERT( maximum_time_until_expiration > block_interval,
                 "Maximum transaction expiration time must be greater than a block interval" );
      FC_ASSERT( maximum_proposal_lifetime - committee_proposal_review_period > block_interval,
                 "Committee proposal review period must be less than the maximum proposal lifetime" );
      
      if( extensions.size() > 0 )
      {
          ext::credit_options new_credit_options = get_credit_options(); 

          FC_ASSERT( new_credit_options.seconds_per_day > 0,
                 "Seconds_per_day interval must be greater than zero" );
          FC_ASSERT( new_credit_options.max_credit_expiration_days >= 0,
                 "Max credit expiration days interval must be greater or equal than zero" );                 
          FC_ASSERT( new_credit_options.min_witnesses_for_exchange_rate > 0,
                  "Min witnesses for set exchange rate must be greater than zero" );                 
          FC_ASSERT( new_credit_options.exchange_rate_set_min_interval > 0,
                  "Exchange rate set min interval must be greater than zero" );                 
          FC_ASSERT( new_credit_options.exchange_rate_set_max_interval > 0,
                  "Exchange rate set max interval must be greater than zero" );
          FC_ASSERT( new_credit_options.exchange_rate_set_min_interval <= new_credit_options.exchange_rate_set_max_interval,
                  "Exchange rate min interval must be less or equal than exchange rate max interval" );
      
          ext::credit_referrer_bonus_options new_bonus_options = get_bonus_options(); 
          FC_ASSERT( new_bonus_options.creditor_referrer_bonus + new_bonus_options.borrower_referrer_bonus  <= new_bonus_options.karma_account_bonus,
                  "The sum of creditor and borrower referrer bonuses must be less or equal than karma account bonus"  );                  
      }            
   }

} } // graphene::chain
