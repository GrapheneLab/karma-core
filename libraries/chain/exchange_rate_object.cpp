#include <graphene/chain/exchange_rate_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/hardfork.hpp>
#include <fc/uint128.hpp>


namespace graphene { namespace chain 
{
    void exchange_rate_object::process( graphene::chain::database* db )
    {       
        uint32_t min_interval_period = db->get_global_properties().parameters.get_credit_options().exchange_rate_set_min_interval;
        uint32_t max_interval_period = db->get_global_properties().parameters.get_credit_options().exchange_rate_set_max_interval;
        uint32_t min_witnesses = db->get_global_properties().parameters.get_credit_options().min_witnesses_for_exchange_rate;
        
        uint32_t now_time = db->head_block_time( ).sec_since_epoch( );
        
        // do we have new_exchange_rates for which its time to check?
        for(auto it_interval = current_exchange_rate_interval.begin(); it_interval != current_exchange_rate_interval.end(); it_interval++)
        {
            //!!! it_interval->second = std::pair<start_time, last_min_interval_check_time>            
            if(now_time - it_interval->second.second >= min_interval_period)    
            {
                uint32_t vec_size = current_exchange_rate[it_interval->first].size();
                bool set_exchange_rates_flag = false;

                // do we have enough votes for this currancy?
                if( vec_size >= min_witnesses)
                {
                    // calculate new median for this currancy
                    std::vector<double> currency;
                    double median;

                    for(auto it = current_exchange_rate[it_interval->first].begin(); it != current_exchange_rate[it_interval->first].end(); it++)
                        currency.push_back(it->second);

                    std::sort(currency.begin(), currency.end());
                
                    if(vec_size%2)
                        median = currency[vec_size/2];
                    else
                        median = (currency[vec_size/2-1]+currency[vec_size/2])/2;
                    
                    last_exchange_rate[it_interval->first]=median;

                    set_exchange_rates_flag = true;
                }
                else // set new last check time
                    it_interval->second.second = now_time;
                
                if( set_exchange_rates_flag || (now_time - it_interval->second.first >= max_interval_period))
                {
                    //clear interval and currant_exchange_rate for this currancy
                    current_exchange_rate[it_interval->first].clear();
                    current_exchange_rate_interval.erase(it_interval);
                }    
            }
        }
    }
    
    double get_exchange_rate_by_symbol( graphene::chain::database* db, std::string symbol)
    {
        const auto& e = db->get_index_type<exchange_rate_index>( ).indices( ).get<by_id>( );    

        auto exchange_rate_itr = (*e.begin()).last_exchange_rate.find(symbol);
        FC_ASSERT( exchange_rate_itr != (*e.begin()).last_exchange_rate.end(), "Exchange rate for ${symbol} has not been set", ("symbol", symbol) );

        return exchange_rate_itr->second;
    }
}}

