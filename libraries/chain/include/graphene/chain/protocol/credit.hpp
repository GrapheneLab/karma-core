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
#include <graphene/chain/protocol/base.hpp>

namespace graphene { namespace chain { 

   /**
    * @brief provides a generic way to add higher level protocols on top of witness consensus
    * @ingroup operations
    *
    * There is no validation for this operation other than that required auths are valid and a fee
    * is paid that is appropriate for the data contained.
    */
   struct credit_request_operation : public base_operation
   {
      struct fee_parameters_type { 
         uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; 
         uint32_t price_per_kbyte = 10;
      };

      asset                     fee;

      account_id_type           borrower; // borrower
      asset                     loan_asset; // sum of credit and currency
      uint32_t                  loan_period; // loan period in month
      uint32_t                  loan_persent; //persent
      std::string               loan_memo; // memo

      asset                     deposit_asset; // sum of deposit and currency   
          
      account_id_type   fee_payer( )const { return borrower; }
      void              validate( )const;
      share_type        calculate_fee( const fee_parameters_type& k )const;
   };
//*************************************************************************************************************************//
   struct credit_approve_operation : public base_operation
   {
      struct fee_parameters_type { 
         uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; 
         uint32_t price_per_kbyte = 10;
      };

      asset                     fee;

      account_id_type           creditor; // creditor
      std::string               credit_memo; // memo
      string credit_request_uuid; // credit_object uuid

      account_id_type   fee_payer()const { return creditor; }
      void              validate()const;
      share_type        calculate_fee(const fee_parameters_type& k)const;
   };
//*************************************************************************************************************************//
   struct credit_request_cancel_operation : public base_operation
   {
      struct fee_parameters_type { 
         uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; 
         uint32_t price_per_kbyte = 10;
      };

      asset                     fee;

      account_id_type           borrower; // borrower
      std::string               borrow_memo; // memo
      string credit_request_uuid; // credit_object uuid

      account_id_type   fee_payer()const { return borrower; }
      void              validate()const;
      share_type        calculate_fee(const fee_parameters_type& k)const;
   };
//*************************************************************************************************************************//
   struct comment_credit_request_operation : public base_operation
   {
      struct fee_parameters_type { 
         uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; 
         uint32_t price_per_kbyte = 10;
      };

      asset                     fee;

      account_id_type           creditor; // creditor
      std::string               credit_memo; // memo
      string credit_request_uuid; // credit_object uuid

      account_id_type   fee_payer()const { return creditor; }
      void              validate()const;
      share_type        calculate_fee(const fee_parameters_type& k)const;
   };
//*************************************************************************************************************************//
   struct settle_credit_operation : public base_operation
   {
      struct fee_parameters_type { 
         uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; 
         uint32_t price_per_kbyte = 10;
      };

      asset                     fee;

      account_id_type           borrower; // borrower
      string                    credit_request_uuid; // credit_object uuid

      account_id_type   fee_payer()const { return borrower; }
      void              validate()const;
      share_type        calculate_fee(const fee_parameters_type& k)const;
   };
//*************************************************************************************************************************//
   //settle credit
/*
   struct credit_failed_operation : public base_operation
   {
      struct fee_parameters_type { 
         uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; 
         uint32_t price_per_kbyte = 10;
      };

      asset                     fee;
      account_id_type           payer;
      flat_set<account_id_type> required_auths;
      uint16_t                  id = 0;
      vector<char>              data;

      account_id_type   fee_payer()const { return payer; }
      void              validate()const;
      share_type        calculate_fee(const fee_parameters_type& k)const;
   };
*/
} } // namespace graphene::chain

FC_REFLECT( graphene::chain::credit_request_operation::fee_parameters_type, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::chain::credit_request_operation, (fee)(borrower)(loan_asset)(loan_period)(loan_persent)(loan_memo)(deposit_asset) )

FC_REFLECT( graphene::chain::credit_approve_operation::fee_parameters_type, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::chain::credit_approve_operation, (fee)(creditor)(credit_memo)(credit_request_uuid) )

FC_REFLECT( graphene::chain::credit_request_cancel_operation::fee_parameters_type, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::chain::credit_request_cancel_operation, (fee)(borrower)(borrow_memo)(credit_request_uuid) )

FC_REFLECT( graphene::chain::comment_credit_request_operation::fee_parameters_type, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::chain::comment_credit_request_operation, (fee)(creditor)(credit_memo)(credit_request_uuid) )

FC_REFLECT( graphene::chain::settle_credit_operation::fee_parameters_type, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::chain::settle_credit_operation, (fee)(borrower)(credit_request_uuid) )


/*
FC_REFLECT( graphene::chain::credit_complete_operation::fee_parameters_type, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::chain::credit_complete_operation, (fee)(payer)(required_auths)(id)(data) )

FC_REFLECT( graphene::chain::credit_failed_operation::fee_parameters_type, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::chain::credit_failed_operation, (fee)(payer)(required_auths)(id)(data) )
*/