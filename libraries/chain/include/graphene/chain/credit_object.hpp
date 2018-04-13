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
#pragma once

#include <graphene/chain/protocol/operations.hpp>
#include <graphene/db/generic_index.hpp>

#include <boost/multi_index/composite_key.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string.hpp>

#include <fc/reflect/reflect.hpp>
#include <graphene/chain/protocol/config.hpp>
#include <graphene/chain/protocol/types.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/evaluator.hpp>

namespace graphene { namespace chain {
   class database;

#define DEPOSIT_PERSENT 300  

#define KARMA_MIN_VALUE 0.0
#define KARMA_MAX_VALUE 5.0
#define KARMA_BONUS_FOR_ACCOUNT_CREATE    1.0
#define KARMA_BONUS_FOR_MONTHLY_PAYMENT   0.05
#define KARMA_BONUS_FOR_CREDIT_PAYMENT    0.2
#define KARMA_PENALTY_FOR_MONTHLY_DELAY  -0.1
#define KARMA_PENALTY_FOR_CREDIT_DEFAULT -0.5

#define SPECIAL_CONVERSION_ACCOUNT "karma"

   struct borrower_info
   {
      account_id_type borrower; // borrower     
      std::string loan_memo; // borrower memo

      asset loan_asset; // sum of credit
      double loan_persent; //persent
      uint32_t loan_period; // loan period in month

      asset deposit_asset; // sum of deposit 
      bool collateral_free;  
   };

   struct creditor_info
   {
      account_id_type creditor; // creditor     
      std::string credit_memo; // creditor memo
      asset credit_asset; // credit asset
      double monthly_payment; // monthly payment
   };

   enum e_credit_object_status
   {
         empty = 0,
         wating_for_acceptance,
         in_progress, 
         cancalled,
         complete_normal,
         complete_ubnormal,
         fetch_all
   };

   class credit_object : public graphene::db::abstract_object<credit_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = credit_object_type;

         std::string object_uuid; 
         e_credit_object_status status;

         borrower_info borrower;
         time_point_sec request_creation_time; 
        
         creditor_info creditor;
         time_point_sec request_approvation_time; 
         uint32_t settle_month_elapsed;

         uint32_t expired_time_start = 0; 

         std::map<string,account_id_type> comments;
         std::vector<string> history;
         std::string history_json;
         
         void              process( graphene::chain::database* db );
         void              on_next_month( graphene::chain::database* db );
         void              settle_monthly_payment( graphene::chain::database* db );
         void              check_expired_pay_time( graphene::chain::database* db );         
         void              settle_from_deposit( graphene::chain::database* db );
         void              complete_credit_operation( graphene::chain::database* db );      
         bool              check_deposit_quotes( graphene::chain::database* db );
         void              complete_credit_operation_ubnormal( graphene::chain::database* db );           

         double            calculate_monthly_payment( double loan_sum, double annual_loan_rate, uint32_t loan_period_in_moths );
   };

   struct by_request_creation_time{};
   struct by_uuid{};

   /**
    * @ingroup object_index
    */
   typedef multi_index_container<
      credit_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
         ordered_non_unique< tag<by_request_creation_time>, member<credit_object, time_point_sec, &credit_object::request_creation_time> >,
         ordered_non_unique< tag<by_uuid>, member<credit_object, string, &credit_object::object_uuid> >
      >
   > credit_multi_index_type;

   /**
    * @ingroup object_index
    */
   typedef generic_index<credit_object, credit_multi_index_type> credit_index;
}}

FC_REFLECT_ENUM( graphene::chain::e_credit_object_status, (empty)(wating_for_acceptance)(in_progress)(cancalled)(complete_normal)(complete_ubnormal)(fetch_all) )

FC_REFLECT( graphene::chain::borrower_info, (borrower)(loan_memo)(loan_asset)(loan_persent)(loan_period)(deposit_asset)(collateral_free) )
FC_REFLECT( graphene::chain::creditor_info, (creditor)(credit_memo)(credit_asset)(monthly_payment) )

FC_REFLECT_DERIVED( graphene::chain::credit_object,
                   ( graphene::db::object ),
                   ( object_uuid )
                   ( status )
                   ( borrower )
                   ( request_creation_time )
                   ( creditor )
                   ( request_approvation_time )
                   ( settle_month_elapsed )
                   ( expired_time_start )
                   ( comments )
                   ( history )
                   ( history_json )
                  )
