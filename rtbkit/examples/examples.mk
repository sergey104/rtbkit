#------------------------------------------------------------------------------#
# examples.mk
# Rémi Attab, 14 Feb 2013
# Copyright (c) 2013 Datacratic.  All rights reserved.
#
# Makefile for various RTBkit examples. 
#------------------------------------------------------------------------------#

#----------------------
$(eval $(call library,augmentor_clicks,augmentor_clicks.cc,augmentor_base rtb bid_request agent_configuration))
$(eval $(call program,augmentor_clicks_runner,augmentor_clicks boost_program_options))
#----------------------
$(eval $(call library,augmentor_fin,augmentor_fin.cc,augmentor_base rtb bid_request agent_configuration))
$(eval $(call program,augmentor_fin_runner,augmentor_fin boost_program_options))
#----------------------
$(eval $(call library,augmentor_start_stop_time,augmentor_start_stop_time.cc,augmentor_base rtb bid_request agent_configuration))
$(eval $(call program,augmentor_start_stop_runner,augmentor_start_stop_time boost_program_options))
#----------------------
$(eval $(call library,augmentor_device_class,augmentor_device_class.cc,augmentor_base rtb bid_request agent_configuration))
$(eval $(call program,augmentor_device_class_runner,augmentor_device_class boost_program_options))
#----------------------
$(eval $(call program,bidding_agent_0.02CPM,bidding_agent rtb_router boost_program_options services))
$(eval $(call program,bidding_agent_0.05CPM,bidding_agent rtb_router boost_program_options services))
$(eval $(call program,bidding_agent_0.15CPM,bidding_agent rtb_router boost_program_options services))
$(eval $(call program,bidding_agent_10CPM,bidding_agent rtb_router boost_program_options services))
$(eval $(call program,bidding_agent_v2,bidding_agent rtb_router boost_program_options services))
$(eval $(call program,bidding_agent_1,bidding_agent rtb_router boost_program_options services))
$(eval $(call library,augmentor_1,augmentor_1.cc,augmentor_base rtb bid_request agent_configuration))
$(eval $(call program,augmentor_1_runner,augmentor_1 boost_program_options))
#----------------------

$(eval $(call library,augmentor_ex,augmentor_ex.cc,augmentor_base rtb bid_request agent_configuration))
$(eval $(call library,augmentor_ag,augmentor_ag.cc,augmentor_base rtb bid_request agent_configuration))
$(eval $(call library,mock_exchange,mock_exchange_connector.cc,exchange))

$(eval $(call program,augmentor_ex_runner,augmentor_ex boost_program_options))
$(eval $(call program,augmentor_ag_runner,augmentor_ag boost_program_options))
$(eval $(call program,data_logger_ex,data_logger data_logger boost_program_options services))
$(eval $(call program,bidding_agent_console,bidding_agent rtb_router boost_program_options services))
$(eval $(call program,bidding_agent_ex,bidding_agent rtb_router boost_program_options services))
$(eval $(call program,bidding_agent_ag,bidding_agent rtb_router boost_program_options services))
$(eval $(call program,bidding_agent_ag1,bidding_agent rtb_router boost_program_options services))
$(eval $(call program,bidding_agent_ag2,bidding_agent rtb_router boost_program_options services))
$(eval $(call program,bid_request_endpoint,exchange rtb_router bidding_agent boost_program_options services))
$(eval $(call program,multi_agent,exchange rtb_router bidding_agent boost_program_options services))
$(eval $(call program,adserver_endpoint,standard_adserver data_logger rtb_router bidding_agent boost_program_options services))
$(eval $(call program,integration_endpoints,exchange standard_adserver data_logger rtb_router bidding_agent boost_program_options services))

RTBKIT_INTEGRATION_TEST_LINK := \
	rtb_router bidding_agent integration_test_utils monitor monitor_service augmentor_ex adserver_connector mock_bid_request mock_adserver

$(eval $(call test,rtbkit_integration_test,$(RTBKIT_INTEGRATION_TEST_LINK),boost))

$(eval $(call program,standalone_bidder_ex,augmentor_base rtb bid_request agent_configuration boost_program_options rtb_router adserver_connector exchange))

LIBRTBKIT_LINK :=                          \
        bidding_agent bid_request          \
        boost_program_options data_logger  \
        exchange rtb_router                \
        services standard_adserver

$(eval $(call library,rtbkit,rtbkit.cc,$(LIBRTBKIT_LINK)))

$(eval $(call include_sub_make,nodeagents))
$(eval $(call include_sub_make,availability_agent,availability_agent,availability_agent.mk))
