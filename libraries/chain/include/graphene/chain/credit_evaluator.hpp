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
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/database.hpp>

#include <map>


//using namespace boost::uuids;

namespace graphene { namespace chain {

   class credit_request_evaluator : public evaluator<credit_request_evaluator>
   {
      public:
         typedef credit_request_operation operation_type;

         void_result do_evaluate( const credit_request_operation& o );
         object_id_type do_apply( const credit_request_operation& o );
   };

   class credit_approve_evaluator : public evaluator<credit_approve_evaluator>
   {
      public:
         typedef credit_approve_operation operation_type;

         void_result do_evaluate( const credit_approve_operation& o );
         object_id_type do_apply( const credit_approve_operation& o );
   };

   class credit_request_cancel_evaluator : public evaluator<credit_request_cancel_evaluator>
   {
      public:
         typedef credit_request_cancel_operation operation_type;

         void_result do_evaluate( const credit_request_cancel_operation& o );
         object_id_type do_apply( const credit_request_cancel_operation& o );
   };  

   class comment_credit_request_evaluator : public evaluator<comment_credit_request_evaluator>
   {
      public:
         typedef comment_credit_request_operation operation_type;

         void_result do_evaluate( const comment_credit_request_operation& o );
         object_id_type do_apply( const comment_credit_request_operation& o );
   };

   class settle_credit_operation_evaluator : public evaluator<settle_credit_operation_evaluator>
   {
      public:
         typedef settle_credit_operation operation_type;

         void_result do_evaluate( const settle_credit_operation& o );
         object_id_type do_apply( const settle_credit_operation& o );
   };      
} } // graphene::chain
