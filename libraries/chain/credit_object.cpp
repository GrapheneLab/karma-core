#include <graphene/chain/credit_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/hardfork.hpp>
#include <fc/uint128.hpp>

#define SECONDS_PER_DAY 86400
#define SECONDS_PER_MONTH 2592000

namespace graphene { namespace chain 
{
    void credit_object::process( graphene::chain::database* db )
    {       
        if( status == e_credit_object_status::in_progress )
        {
            boost::gregorian::date today = boost::posix_time::from_time_t( db->head_block_time( ).sec_since_epoch( ) ).date( );
            boost::gregorian::date approvation = boost::posix_time::from_time_t( request_approvation_time.sec_since_epoch( ) ).date( );

            uint32_t months_elapsed = ( today.year( ) - approvation.year( ) ) * 12 + today.month( ) - approvation.month( );

            check_deposit_quotes( db );
            on_next_month( db );

            if( settle_month_elapsed >= borrower.loan_period )
                complete_credit_operation( db );

            // release mode:
            /*
            if( settle_month_elapsed != months_elapsed )
            {
                on_next_month( db );
                settle_month_elapsed = months_elapsed;

                if( settle_month_elapsed >= borrower.loan_period )
                    complete_credit_operation( db );
            } 
            */
            
        }                  
    }

    void credit_object::on_next_month( graphene::chain::database* db )
    {
        const database& d = *db;
        const account_object& borrower_account = borrower.borrower( d );     
        const asset_object& loan_asset_type = borrower.loan_asset.asset_id( d );          

        asset monthly_payment( creditor.monthly_payment * 100000, loan_asset_type.get_id( ) );       
        bool sufficient_balance = db->get_balance( borrower_account, loan_asset_type ) >= monthly_payment;
        if( sufficient_balance )
            settle_monthly_payment( creditor.monthly_payment * 100000, db );       
        else
            settle_from_deposit( creditor.monthly_payment, db );

        settle_month_elapsed++;
    }

    double credit_object::calculate_monthly_payment( double loan_sum, double annual_loan_rate, uint32_t loan_period_in_moths )
    {
        double p = ( annual_loan_rate / 12 ) / 100;
        double n = loan_period_in_moths;
        double t = pow( ( 1 + p ), n ) - 1;

        return loan_sum * ( p + p / t );    
    }

    void credit_object::settle_monthly_payment( double sattle_amount, graphene::chain::database* db )
    {
        const database& d = *db;
        const asset_object& loan = borrower.loan_asset.asset_id( d );  
        const asset_object& deposit = borrower.deposit_asset.asset_id( d ); 

        asset tmp( sattle_amount, loan.get_id( ) ); 
        db->adjust_balance( borrower.borrower, -tmp );
        db->adjust_balance( creditor.creditor, tmp );

        std::stringstream ss;
        ss << db->head_block_time( ).to_iso_string( ) << " : " << "Settle monthly payment complete normal, " << loan.amount_to_pretty_string( tmp ) << " settled."
        << " month elapsed - " << settle_month_elapsed + 1
        << " deposit left = " << deposit.amount_to_pretty_string( borrower.deposit_asset );
        history.push_back( ss.str( ) );
    }

    void credit_object::settle_from_deposit( double sattle_amount, graphene::chain::database* db )
    {
        database& d = *db;

        const asset_object& deposit = borrower.deposit_asset.asset_id( d );
        const asset_object& loan = borrower.loan_asset.asset_id( d );

        double loan_left = loan.amount_to_real( db->get_balance( borrower.borrower, loan.get_id( ) ).amount );
        double deposit_left = deposit.amount_to_real( db->get_balance( borrower.borrower, deposit.get_id( ) ).amount );

        if( deposit_left == 0.0 || borrower.collateral_free )
        {
            complete_credit_operation_ubnormal( db );
            return;
        }
        
        double deposit_to_core;
        double loan_to_core;

        if( loan_left > 0.0 )
        {
            asset tmp( loan_left * 100000, loan.get_id( ) );
             
            db->adjust_balance( borrower.borrower, -tmp );
            db->adjust_balance( creditor.creditor, tmp );

            deposit_to_core = ( sattle_amount - ( loan_left * loan.options.core_exchange_rate.to_real( ) ) ) * deposit.options.core_exchange_rate.to_real( );
            loan_to_core = sattle_amount * loan.options.core_exchange_rate.to_real( );
        }else
        {
            deposit_to_core = sattle_amount * deposit.options.core_exchange_rate.to_real( );
            loan_to_core = sattle_amount * loan.options.core_exchange_rate.to_real( );        
        }

        asset deposit_tmp( deposit_to_core * 100000, deposit.get_id( ) );
        asset loan_tmp( loan_to_core * 100000, loan.get_id( ) );

        borrower.deposit_asset -= deposit_tmp;
        db->adjust_balance( creditor.creditor, loan_tmp );   

        std::stringstream ss;
        ss << db->head_block_time( ).to_iso_string( ) << " : " << "Settle monthly payment complete ubnormal, " << deposit.amount_to_pretty_string( deposit_tmp ) << " settled. "
        << loan.amount_to_pretty_string( loan_tmp ) << " transfered " 
        << " deposit to core = " << deposit_to_core << " loan to core = " << loan_to_core
        << " month elapsed = " << settle_month_elapsed + 1
        << " deposit left = " << deposit.amount_to_pretty_string( borrower.deposit_asset );
        history.push_back( ss.str( ) );
    }
         
    void credit_object::complete_credit_operation( graphene::chain::database* db )
    {
        database& d = *db;

        const asset_object& deposit = borrower.deposit_asset.asset_id( d );
        db->adjust_balance( borrower.borrower, borrower.deposit_asset );
        
        std::stringstream ss;
        ss << db->head_block_time( ).to_iso_string( ) << " : " << "Credit complete normal, " << deposit.amount_to_pretty_string( borrower.deposit_asset ) << " returned.";
        history.push_back( ss.str( ) );

        borrower.deposit_asset = asset( 0, deposit.get_id( ) );
        status = e_credit_object_status::complete_normal;
    }

    void credit_object::complete_credit_operation_ubnormal( graphene::chain::database* db )
    {
        database& d = *db;

        const asset_object& deposit = borrower.deposit_asset.asset_id( d );
        db->adjust_balance( borrower.borrower, borrower.deposit_asset );
        
        std::stringstream ss;
        ss << db->head_block_time( ).to_iso_string( ) << " : " << "Credit ubnormal normal, " << deposit.amount_to_pretty_string( borrower.deposit_asset ) << " returned.";
        history.push_back( ss.str( ) );

        borrower.deposit_asset = asset( 0, deposit.get_id( ) );
        status = e_credit_object_status::complete_ubnormal;
    }

    void credit_object::check_deposit_quotes( graphene::chain::database* db )
    {
        if( borrower.collateral_free )
            return;
            
        database& d = *db;
        const asset_object& deposit = borrower.deposit_asset.asset_id( d );      
        const asset_object& loan = borrower.loan_asset.asset_id( d );

        double deposit_in_core = deposit.amount_to_real( borrower.deposit_asset.amount ) * deposit.options.core_exchange_rate.to_real( );
        double loan_in_core = loan.amount_to_real( borrower.loan_asset.amount ) * loan.options.core_exchange_rate.to_real( );  

        if( deposit_in_core < ( loan_in_core / 100 ) * ( DEPOSIT_PERSENT / 2 ) )
        {
            std::stringstream ss;
            ss << db->head_block_time( ).to_iso_string( ) << " : " << "Stop-Loss Order, deposit in KRM = " << deposit_in_core 
            << " " << ( DEPOSIT_PERSENT / 2 ) << "% of loan in KRM = " << ( loan_in_core / 100 ) * ( DEPOSIT_PERSENT / 2 );
        
            status = e_credit_object_status::complete_ubnormal;
            double debt = creditor.monthly_payment * ( borrower.loan_period - settle_month_elapsed + 1 );
            double loan_left = loan.amount_to_real( db->get_balance( borrower.borrower, loan.get_id( ) ).amount );

            if( loan_left >= debt )
            {
                asset tmp( loan_left * 100000, loan.get_id( ) ); 
                db->adjust_balance( borrower.borrower, -tmp );
                db->adjust_balance( creditor.creditor, tmp );
                db->adjust_balance( borrower.borrower, borrower.deposit_asset );
                borrower.deposit_asset = asset( 0, deposit.get_id( ) );

                ss << "deposit - " << deposit.amount_to_pretty_string( borrower.deposit_asset ) << " returned.";
            }else
            {
                double diff = debt - loan_left;
                asset tmp( loan_left * 100000, loan.get_id( ) ); 
                db->adjust_balance( borrower.borrower, -tmp );
                db->adjust_balance( creditor.creditor, tmp );

                double loan_in_core = diff * loan.options.core_exchange_rate.to_real( );
                double debt_in_deposit = loan_in_core * deposit.options.core_exchange_rate.to_real( );
                double deposit_total = deposit.amount_to_real( db->get_balance( borrower.borrower, deposit.get_id( ) ).amount );

                if( deposit_total > debt_in_deposit )
                {
                    asset tmp( debt_in_deposit * 100000, deposit.get_id( ) ); 
                    borrower.deposit_asset -= tmp;
                    db->adjust_balance( borrower.borrower, borrower.deposit_asset );
                    db->adjust_balance( creditor.creditor, tmp ); 
                    
                    ss << "deposit - " << deposit.amount_to_pretty_string( borrower.deposit_asset ) << " returned.";
                    borrower.deposit_asset = asset( 0, deposit.get_id( ) );
                }else
                {
                    asset tmp( deposit_total * 100000, deposit.get_id( ) ); 
                    db->adjust_balance( borrower.borrower, -tmp );
                    db->adjust_balance( creditor.creditor, tmp ); 
                    borrower.deposit_asset = asset( 0, deposit.get_id( ) );

                    ss << "deposit - " << 0 << " returned.";
                }
            }
            history.push_back( ss.str( ) );          
        }
    }
}}