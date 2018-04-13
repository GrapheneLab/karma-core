#include <graphene/chain/credit_object.hpp>
#include <graphene/chain/exchange_rate_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/hardfork.hpp>
#include <fc/uint128.hpp>

namespace graphene { namespace chain 
{
    void credit_object::process( graphene::chain::database* db )
    {       
        if( status == e_credit_object_status::in_progress )
        {
            if( borrower.collateral_free == false )
                if( check_deposit_quotes( db ) == false)
                        return; // credit already closed ubnormal
            
            uint32_t seconds_per_day = db->get_global_properties().parameters.get_credit_options().seconds_per_day;
                        
            boost::posix_time::ptime         credit_start_time = boost::posix_time::from_time_t( request_approvation_time.sec_since_epoch( ) );
            boost::gregorian::month_iterator itr( credit_start_time.date() - boost::gregorian::date_duration(1), settle_month_elapsed+1);
            boost::gregorian::date           pay_date = *(++itr);
            
            // next pay day from request_approvation_time.
            int next_pay_count_days = boost::gregorian::date_period(credit_start_time.date(),pay_date).length().days();
            long next_pay_total_secs = next_pay_count_days * seconds_per_day;

            boost::posix_time::ptime cur_time = boost::posix_time::from_time_t( db->head_block_time( ).sec_since_epoch( ) );

            if( (cur_time - credit_start_time).total_seconds() >= next_pay_total_secs || expired_time_start)
            {
                on_next_month( db );

                if( settle_month_elapsed >= borrower.loan_period )
                    complete_credit_operation( db );
            }
        }                  
    }

    void credit_object::on_next_month( graphene::chain::database* db )
    {
        const account_object& borrower_account = borrower.borrower( *db );     
        const asset_object& loan_asset_type = borrower.loan_asset.asset_id( *db );          

        asset monthly_payment( creditor.monthly_payment * pow(10, loan_asset_type.precision), loan_asset_type.get_id( ) );       
        bool sufficient_balance = db->get_balance( borrower_account, loan_asset_type ) >= monthly_payment;

        if( sufficient_balance )
            settle_monthly_payment( db );       
        else 
            check_expired_pay_time( db );

        if( status == e_credit_object_status::complete_ubnormal)
            return;

        if(!expired_time_start)
            settle_month_elapsed++;
    }

    double credit_object::calculate_monthly_payment( double loan_sum, double annual_loan_rate, uint32_t loan_period_in_moths )
    {
        double p = ( annual_loan_rate / 12 ) / 100;
        double n = loan_period_in_moths;
        double t = pow( ( 1 + p ), n ) - 1;

        return loan_sum * ( p + p / t );    
    }

    void credit_object::settle_monthly_payment( graphene::chain::database* db )
    {
        const asset_object& loan = borrower.loan_asset.asset_id( *db );  
        const asset_object& deposit = borrower.deposit_asset.asset_id( *db ); 

        asset tmp( creditor.monthly_payment * pow(10, loan.precision), loan.get_id( ) ); 
        db->adjust_balance( borrower.borrower, -tmp );
        db->adjust_balance( creditor.creditor, tmp );

        update_account_karma(db, borrower.borrower, KARMA_BONUS_FOR_MONTHLY_PAYMENT, "Settle monthly payment complete normal.");

        expired_time_start = 0;

        std::stringstream ss;
        std::string time_json = db->head_block_time( ).to_iso_string( );
        ss << time_json << " : " << "Settle monthly payment complete normal, " << loan.amount_to_pretty_string( tmp ) << " settled."
        << " month elapsed - " << settle_month_elapsed + 1
        << " deposit left = " << deposit.amount_to_pretty_string( borrower.deposit_asset );
        history.push_back( ss.str( ) );

        boost::property_tree::ptree root, details;
        std::stringstream ss_json;

        if(history_json.size())
        {
            ss_json << history_json;
            boost::property_tree::read_json(ss_json, root);    
            ss_json.str( std::string() );
            ss_json.clear();
        }
        details.put("type","monthly_settlement");
        details.put("status","normal");
        details.put("sum_settled",loan.amount_to_string( tmp ));
        details.put("asset_settled",loan.symbol);
        details.put("month_elapsed",settle_month_elapsed + 1);
        details.put("deposit_asset",deposit.symbol);
        details.put("deposit_left_sum",deposit.amount_to_string(borrower.deposit_asset.amount));
        root.add_child(time_json, details);

        boost::property_tree::write_json(ss_json, root);
        history_json = ss_json.str();
        history_json.erase(std::remove(history_json.begin(), history_json.end(), '\n'), history_json.end());
        history_json.erase(std::remove(history_json.begin(), history_json.end(), ' '), history_json.end());
    }

    void credit_object::check_expired_pay_time( graphene::chain::database* db )
    {
        uint32_t max_days = db->get_global_properties().parameters.get_credit_options().max_credit_expiration_days;
        if ( (expired_time_start == 0) && (max_days != 0) )
        {
            const asset_object& loan = borrower.loan_asset.asset_id( *db );  
            asset tmp( creditor.monthly_payment * pow(10, loan.precision), loan.get_id( ) ); 
         
            expired_time_start = db->head_block_time( ).sec_since_epoch( );

            update_account_karma(db, borrower.borrower, KARMA_PENALTY_FOR_MONTHLY_DELAY, "Settle monthly payment complete ubnormal: insufficient balance.");

            std::stringstream ss;
            std::string time_json = db->head_block_time( ).to_iso_string( );
            ss << time_json << " : " << "Settle monthly payment complete ubnormal: insufficient balance. " 
            << loan.amount_to_pretty_string( tmp ) << " not settled.";
            history.push_back( ss.str( ) );

            boost::property_tree::ptree root, details;
            std::stringstream ss_json;

            if(history_json.size())
            {
                ss_json << history_json;
                boost::property_tree::read_json(ss_json, root);    
                ss_json.str( std::string() );
                ss_json.clear();
            }
            details.put("type","monthly_settlement");
            details.put("status","insufficient_balance");
            details.put("sum_not_settled",loan.amount_to_string( tmp ));
            details.put("asset",loan.symbol);
            root.add_child(time_json, details);

            boost::property_tree::write_json(ss_json, root);
            history_json = ss_json.str();
            history_json.erase(std::remove(history_json.begin(), history_json.end(), '\n'), history_json.end());
            history_json.erase(std::remove(history_json.begin(), history_json.end(), ' '), history_json.end());
            return; 
        }    
        
        uint32_t max_credit_expiration_days = db->get_global_properties().parameters.get_credit_options().max_credit_expiration_days;
        uint32_t max_credit_expiration_time = max_credit_expiration_days * db->get_global_properties().parameters.get_credit_options().seconds_per_day;
        
        if( (db->head_block_time( ).sec_since_epoch( ) - expired_time_start) >= max_credit_expiration_time)
        {
            if(borrower.collateral_free)
                complete_credit_operation_ubnormal( db );
            else    
                settle_from_deposit( db );
        }    
    }    

    void credit_object::settle_from_deposit( graphene::chain::database* db )
    {
        const asset_object& deposit = borrower.deposit_asset.asset_id( *db );
        const asset_object& loan = borrower.loan_asset.asset_id( *db );

        double deposit_left = deposit.amount_to_real( borrower.deposit_asset.amount );

        if( deposit_left == 0.0 )
            return complete_credit_operation_ubnormal( db );

        double deposit_exchange_rate = get_exchange_rate_by_symbol(db, deposit.symbol);
        double loan_exchange_rate = get_exchange_rate_by_symbol(db, loan.symbol);

        double loan_left_on_borrower_account = loan.amount_to_real( db->get_balance( borrower.borrower, loan.get_id( ) ).amount );
        double debt_in_deposit = (creditor.monthly_payment - loan_left_on_borrower_account) * loan_exchange_rate/deposit_exchange_rate;

        double deposit_to_core = debt_in_deposit * deposit_exchange_rate;
        double loan_to_core = creditor.monthly_payment * loan_exchange_rate;

        // try return settle_amount = loan_left_on_borrower_account + debt_in_deposit
        asset loan_asset_from_borrower(loan_left_on_borrower_account * pow(10, loan.precision),loan.get_id());
        asset deposit_asset_from_deposit(debt_in_deposit * pow(10, deposit.precision), deposit.get_id());
        asset loan_asset_to_creditor(creditor.monthly_payment * pow(10, loan.precision),loan.get_id());

        bool fail = false;   
        //We shouldn`t be here. We can`t return sattle_amount sum. We use all deposit to return as much as possible.
        if( deposit_left < debt_in_deposit )
        {
            double loan_from_all_deposit = deposit_left * deposit_exchange_rate / loan_exchange_rate;
            loan_asset_to_creditor.amount = (loan_from_all_deposit + loan_left_on_borrower_account) * pow(10, loan.precision);
            deposit_asset_from_deposit.amount = deposit_left * pow(10, deposit.precision);    
            fail = true;
        }
        
        db->adjust_balance( borrower.borrower, -loan_asset_from_borrower );
        db->adjust_balance( creditor.creditor, loan_asset_to_creditor );
        borrower.deposit_asset -= deposit_asset_from_deposit;
        
        // check convertation
        if(loan_asset_from_borrower.asset_id != loan_asset_to_creditor.asset_id)
        {
            auto itr_account_obj = db->get_index_type<account_index>().indices().get<by_name>().find(SPECIAL_CONVERSION_ACCOUNT);
            account_id_type karma_id = (*itr_account_obj).get_id();
            db->adjust_balance( karma_id, deposit_asset_from_deposit );
        
            double loan_need_to_be_balanced = deposit.amount_to_real(deposit_asset_from_deposit.amount) * deposit_exchange_rate/loan_exchange_rate;
            db->modify(loan.dynamic_asset_data_id(*db), [&](asset_dynamic_data_object& dynamic_asset) {
            dynamic_asset.current_supply += loan_need_to_be_balanced*pow(10,loan.precision);
            });
        }    

        std::stringstream ss;
        std::string time_json = db->head_block_time( ).to_iso_string( );
        ss << time_json << " : " << "Settle monthly payment complete ubnormal, " 
        << loan.amount_to_pretty_string( loan_asset_from_borrower ) << " settled from borrower. "
        << deposit.amount_to_pretty_string( deposit_asset_from_deposit ) << " settled from deposit. "
        << loan.amount_to_pretty_string( loan_asset_to_creditor ) << " transfered " 
        << " deposit to core = " << deposit_to_core << " loan to core = " << loan_to_core
        << " month elapsed = " << settle_month_elapsed + 1
        << " deposit left = " << deposit.amount_to_pretty_string( borrower.deposit_asset );
        history.push_back( ss.str( ) );

        boost::property_tree::ptree root, details;
        std::stringstream ss_json;

        if(history_json.size())
        {
            ss_json << history_json;
            boost::property_tree::read_json(ss_json, root);    
            ss_json.str( std::string() );
            ss_json.clear();
        }

        details.put("type","monthly_settlement");
        details.put("status","ubnormal");
        details.put("settled_from_borrower_asset",loan.symbol);
        details.put("settled_from_borrower_sum",loan.amount_to_string( loan_asset_from_borrower ));
        details.put("settled_from_deposit_asset",deposit.symbol);
        details.put("settled_from_deposit_sum",deposit.amount_to_string( deposit_asset_from_deposit ));
        details.put("transfered_sum",loan.amount_to_string( loan_asset_to_creditor ));
        details.put("transfered_asset",loan.symbol);
        details.put("deposit_to_core",deposit_to_core);
        details.put("loan_to_core",loan_to_core);
        details.put("month_elapsed",settle_month_elapsed + 1);
        details.put("deposit_asset",deposit.symbol);
        details.put("deposit_left_sum",deposit.amount_to_string( borrower.deposit_asset ));
        root.add_child(time_json, details);

        boost::property_tree::write_json(ss_json, root);
        history_json = ss_json.str();
        history_json.erase(std::remove(history_json.begin(), history_json.end(), '\n'), history_json.end());
        history_json.erase(std::remove(history_json.begin(), history_json.end(), ' '), history_json.end());

        if(fail)
            return complete_credit_operation_ubnormal(db);
        
        expired_time_start = 0;    
    }
         
    void credit_object::complete_credit_operation( graphene::chain::database* db )
    {
        const asset_object& deposit = borrower.deposit_asset.asset_id( *db );
        db->adjust_balance( borrower.borrower, borrower.deposit_asset );
        
        update_account_karma(db, borrower.borrower, KARMA_BONUS_FOR_CREDIT_PAYMENT, "Credit complete normal.");

        std::stringstream ss;
        std::string time_json = db->head_block_time( ).to_iso_string( );
        ss << time_json << " : " << "Credit complete normal, " << deposit.amount_to_pretty_string( borrower.deposit_asset ) << " returned.";
        history.push_back( ss.str( ) );

        boost::property_tree::ptree root, details;
        std::stringstream ss_json;

        if(history_json.size())
        {
            ss_json << history_json;
            boost::property_tree::read_json(ss_json, root);    
            ss_json.str( std::string() );
            ss_json.clear();
        }

        details.put("type","credit_complete");
        details.put("status","normal");
        details.put("returned_deposit_sum",deposit.amount_to_string( borrower.deposit_asset.amount));
        details.put("returned_deposit_asset",deposit.symbol);
        root.add_child(time_json, details);

        boost::property_tree::write_json(ss_json, root);
        history_json = ss_json.str();
        history_json.erase(std::remove(history_json.begin(), history_json.end(), '\n'), history_json.end());
        history_json.erase(std::remove(history_json.begin(), history_json.end(), ' '), history_json.end());

        borrower.deposit_asset = asset( 0, deposit.get_id( ) );
        status = e_credit_object_status::complete_normal;
    }

    void credit_object::complete_credit_operation_ubnormal( graphene::chain::database* db )
    {
        // We don`t have deposit. Return all what we have on borrower account in loan asset.
        const asset_object& deposit = borrower.deposit_asset.asset_id( *db );
        const asset_object& loan = borrower.loan_asset.asset_id( *db );

        double loan_left_on_borrower_account = loan.amount_to_real( db->get_balance( borrower.borrower, loan.get_id( ) ).amount );
        asset loan_asset_to_creditor( loan_left_on_borrower_account * pow(10, loan.precision), loan.get_id( ) );

        db->adjust_balance( borrower.borrower, -loan_asset_to_creditor );
        db->adjust_balance( creditor.creditor, loan_asset_to_creditor );

        update_account_karma(db, borrower.borrower, KARMA_PENALTY_FOR_CREDIT_DEFAULT, "Credit complete ubnormal.");

        std::stringstream ss;
        std::string time_json = db->head_block_time( ).to_iso_string( );
        ss << time_json << " : " << "Credit complete ubnormal, " << loan.amount_to_pretty_string( loan_asset_to_creditor ) << " transfered."
        << " Deposit sum returned: " << deposit.amount_to_pretty_string( borrower.deposit_asset );
        history.push_back( ss.str( ) );

        boost::property_tree::ptree root, details;
        std::stringstream ss_json;

        if(history_json.size())
        {
            ss_json << history_json;
            boost::property_tree::read_json(ss_json, root);    
            ss_json.str( std::string() );
            ss_json.clear();
        }        

        details.put("type","credit_complete");
        details.put("status","ubnormal");
        details.put("transfered_sum",loan.amount_to_string( loan_asset_to_creditor));
        details.put("transfered_asset",loan.symbol);
        details.put("deposit_sum_returned",deposit.amount_to_string(borrower.deposit_asset));
        details.put("deposit_asset",deposit.symbol);
        root.add_child(time_json, details);

        boost::property_tree::write_json(ss_json, root);
        history_json = ss_json.str();
        history_json.erase(std::remove(history_json.begin(), history_json.end(), '\n'), history_json.end());
        history_json.erase(std::remove(history_json.begin(), history_json.end(), ' '), history_json.end());

        status = e_credit_object_status::complete_ubnormal;
    }

    bool credit_object::check_deposit_quotes( graphene::chain::database* db )
    {
        const asset_object& deposit = borrower.deposit_asset.asset_id( *db );      
        const asset_object& loan = borrower.loan_asset.asset_id( *db );

        double deposit_exchange_rate = get_exchange_rate_by_symbol(db, deposit.symbol);
        double loan_exchange_rate = get_exchange_rate_by_symbol(db, loan.symbol);

        double deposit_in_core = deposit.amount_to_real( borrower.deposit_asset.amount ) * deposit_exchange_rate;
        double loan_in_core = loan.amount_to_real( borrower.loan_asset.amount ) * loan_exchange_rate;  

        if( deposit_in_core > ( loan_in_core / 100 ) * ( DEPOSIT_PERSENT / 2 ) )
            return true;
        
        // Complete credit Stop-Loss Order
        double loan_for_return = creditor.monthly_payment * ( borrower.loan_period - settle_month_elapsed );
        double loan_left_on_borrower_account = loan.amount_to_real( db->get_balance( borrower.borrower, loan.get_id( ) ).amount );

        // ideal situation - we have enough loan asset on borrower account
        asset loan_asset_from_borrower(loan_for_return * pow(10, loan.precision),loan.get_id());
        asset loan_asset_to_creditor(loan_asset_from_borrower);
        asset deposit_asset_from_deposit(0, deposit.get_id());
   
        // if situation not ideal
        if( loan_left_on_borrower_account < loan_for_return )
        {
            loan_asset_from_borrower.amount = loan_left_on_borrower_account * pow(10, loan.precision); 
            double debt_in_deposit = (loan_for_return - loan_left_on_borrower_account) * loan_exchange_rate/deposit_exchange_rate;
            deposit_asset_from_deposit.amount = debt_in_deposit * pow(10, deposit.precision);
        
            double deposit_left = deposit.amount_to_real( borrower.deposit_asset.amount );

            // We can`t return all credit sum. We use all deposit to return as much as possible.
            if( deposit_left < debt_in_deposit )
            {
                double loan_from_all_deposit = deposit_left * deposit_exchange_rate / loan_exchange_rate;
                loan_asset_to_creditor.amount = (loan_from_all_deposit + loan_left_on_borrower_account) * pow(10, loan.precision);
                deposit_asset_from_deposit.amount = deposit_left * pow(10, deposit.precision);    
            }
        }    

        db->adjust_balance( borrower.borrower, -loan_asset_from_borrower );
        db->adjust_balance( creditor.creditor, loan_asset_to_creditor );
        borrower.deposit_asset -= deposit_asset_from_deposit;
        
        if(loan_asset_from_borrower.asset_id != loan_asset_to_creditor.asset_id)
        {
            auto itr_account_obj = db->get_index_type<account_index>().indices().get<by_name>().find(SPECIAL_CONVERSION_ACCOUNT);
            account_id_type karma_id = (*itr_account_obj).get_id();
            db->adjust_balance( karma_id, deposit_asset_from_deposit );

            double loan_need_to_be_balanced = deposit.amount_to_real(deposit_asset_from_deposit.amount) * deposit_exchange_rate/loan_exchange_rate;
            db->modify(loan.dynamic_asset_data_id(*db), [&](asset_dynamic_data_object& dynamic_asset) {
            dynamic_asset.current_supply += loan_need_to_be_balanced*pow(10,loan.precision);
            });
        }    
        
        db->adjust_balance( borrower.borrower, borrower.deposit_asset );

        boost::property_tree::ptree root, details;
        std::stringstream ss, ss_json;
        std::string time_json = db->head_block_time( ).to_iso_string( );

        ss << time_json << " : " << "Stop-Loss Order, deposit in core = " << deposit_in_core 
        << " less then " << ( DEPOSIT_PERSENT / 2 ) << "% of loan in core = " << ( loan_in_core / 100 ) * ( DEPOSIT_PERSENT / 2 )
        << " " << loan.amount_to_pretty_string( loan_asset_from_borrower ) << " settled from borrower. "
        << deposit.amount_to_pretty_string( deposit_asset_from_deposit ) << " settled from deposit. "
        << loan.amount_to_pretty_string( loan_asset_to_creditor ) << " transfered " 
        << "deposit - " << deposit.amount_to_pretty_string( borrower.deposit_asset ) << " returned.";

        if(history_json.size())
        {
            ss_json << history_json;
            boost::property_tree::read_json(ss_json, root);    
            ss_json.str( std::string() );
            ss_json.clear();
        }        

        details.put("type","stop-loss_order");
        details.put("deposit_in_core",deposit_in_core);
        std::string json_tmp_str = std::to_string(DEPOSIT_PERSENT / 2) + "%_of_loan_in_core";
        details.put(json_tmp_str,( loan_in_core / 100 ) * ( DEPOSIT_PERSENT / 2 ));
        details.put("settled_from_borrower_asset",loan.symbol);
        details.put("settled_from_borrower_sum",loan.amount_to_string( loan_asset_from_borrower ));
        details.put("settled_from_deposit_asset",deposit.symbol);
        details.put("settled_from_deposit_sum",deposit.amount_to_string( deposit_asset_from_deposit ));
        details.put("transfered_sum",loan.amount_to_string( loan_asset_to_creditor ));
        details.put("transfered_asset",loan.symbol);
        details.put("deposit_returned_asset",deposit.symbol);
        details.put("deposit_returned_sum",deposit.amount_to_string( borrower.deposit_asset ));
        history.push_back( ss.str( ) );          
        root.add_child(time_json, details);
        boost::property_tree::write_json(ss_json, root);
        history_json = ss_json.str();
        history_json.erase(std::remove(history_json.begin(), history_json.end(), '\n'), history_json.end());
        history_json.erase(std::remove(history_json.begin(), history_json.end(), ' '), history_json.end());

        borrower.deposit_asset = asset( 0, deposit.get_id( ) );
        status = e_credit_object_status::complete_ubnormal;
        return false;
    }
}}