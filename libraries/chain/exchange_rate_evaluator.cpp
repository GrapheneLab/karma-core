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
#include <graphene/chain/exchange_rate_evaluator.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/exchange_rate_object.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
#include <graphene/chain/witness_object.hpp>

#include <iostream>
#include <string>

namespace graphene { namespace chain {
//***************************************************************************************************************************************//
//***************************************************************************************************************************************//
//***************************************************************************************************************************************//

void_result exchange_rate_set_evaluator::do_evaluate( const exchange_rate_set_operation& op )
{ 

   try {
        const auto& witness_idx = db().get_index_type<witness_index>().indices().get<by_account>();
        auto itr_witnesses = witness_idx.find(op.witness);

        if(itr_witnesses == witness_idx.end())
            FC_THROW("Only witness can set exchange rate!");

        // found witness_object!
        witness_object wo = *itr_witnesses;

        // Check if it is active witness
        auto itr_active_witnesses = db().get_global_properties().active_witnesses.find(wo.id);

        if(itr_active_witnesses == db().get_global_properties().active_witnesses.end())
            FC_THROW("Only active witness can set exchange rate!");

        const auto& assets_by_symbol = db().get_index_type<asset_index>().indices().get<by_symbol>();
   
        for(auto it = op.exchange_rate.begin(); it != op.exchange_rate.end(); it++)
        {
            auto asset_symbol_itr = assets_by_symbol.find( it->first );

            if(it->first == GRAPHENE_SYMBOL)
                FC_ASSERT(0, "It is forbidden to set exchange rate for core asset: ${asset}!", ("asset",it->first));

            FC_ASSERT(asset_symbol_itr != assets_by_symbol.end(), "Could not find asset matching ${asset}", ("asset", it->first));
            FC_ASSERT(it->second > 0.0, "Exchange rate for ${asset} must be greater then zero!", ("asset",it->first));
        }

        return void_result( );

}  FC_CAPTURE_AND_RETHROW( ( op ) ) }

void_result exchange_rate_set_evaluator::do_apply( const exchange_rate_set_operation& op )
{ 
   try {          
   
        const auto& e = db( ).get_index_type<exchange_rate_index>( ).indices( ).get<by_id>( );
   
        db( ).modify( *e.begin(), [this,&op]( exchange_rate_object& b ) 
        {
            for(auto it_new_exchange_rates = op.exchange_rate.begin(); it_new_exchange_rates != op.exchange_rate.end(); it_new_exchange_rates ++)
            {
                b.current_exchange_rate[it_new_exchange_rates->first][op.witness] = it_new_exchange_rates->second;

                // is this the first set for this currancy?    
                auto it_exchange_rate_interval = b.current_exchange_rate_interval.find(it_new_exchange_rates->first);
                if(it_exchange_rate_interval == b.current_exchange_rate_interval.end())
                {
                    std::pair<double, double> pair_intervals(db( ).head_block_time( ).sec_since_epoch( ),db( ).head_block_time( ).sec_since_epoch( ));
                    b.current_exchange_rate_interval[it_new_exchange_rates->first] = pair_intervals;
                }
            }    
        });

        return void_result();

} FC_CAPTURE_AND_RETHROW( ( op ) ) }

} } // graphene::chain
