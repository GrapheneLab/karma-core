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

#include <fc/reflect/reflect.hpp>
#include <graphene/chain/protocol/config.hpp>
#include <graphene/chain/protocol/types.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/evaluator.hpp>

namespace graphene { namespace chain {
   class database;

   class exchange_rate_object : public graphene::db::abstract_object<exchange_rate_object>
   {
      public:
        static const uint8_t space_id = protocol_ids;
        static const uint8_t type_id  = exchange_rate_object_type;

        std::map< std::string, std::map< account_id_type, double >> current_exchange_rate;
        std::map< std::string, std::pair<uint32_t, uint32_t>> current_exchange_rate_interval;// "asset": <first_set_exchange_rate_time, last_view_witnesses>
        std::map< std::string, double> last_exchange_rate; 

        void   process(graphene::chain::database* db);
   };

   /**
    * @ingroup object_index
    */
   typedef multi_index_container<
      exchange_rate_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >
      >
   > exchange_rate_multi_index_type;

   /**
    * @ingroup object_index
    */
   typedef generic_index<exchange_rate_object, exchange_rate_multi_index_type> exchange_rate_index;

   double get_exchange_rate_by_symbol( graphene::chain::database* db, std::string symbol );
}}



FC_REFLECT_DERIVED( graphene::chain::exchange_rate_object,
                   ( graphene::db::object ),
                   ( current_exchange_rate )
                   ( current_exchange_rate_interval)
                   ( last_exchange_rate )
                  )
