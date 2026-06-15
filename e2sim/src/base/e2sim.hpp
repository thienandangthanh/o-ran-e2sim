/*****************************************************************************
#                                                                            *
# Copyright 2020 AT&T Intellectual Property                                  *
# Copyright (c) 2020 Samsung Electronics Co., Ltd. All Rights Reserved.      *
#                                                                            *
# Licensed under the Apache License, Version 2.0 (the "License");            *
# you may not use this file except in compliance with the License.           *
# You may obtain a copy of the License at                                    *
#                                                                            *
#      http://www.apache.org/licenses/LICENSE-2.0                            *
#                                                                            *
# Unless required by applicable law or agreed to in writing, software        *
# distributed under the License is distributed on an "AS IS" BASIS,          *
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
# See the License for the specific language governing permissions and        *
# limitations under the License.                                             *
#                                                                            *
******************************************************************************/

#ifndef E2SIM_HPP
#define E2SIM_HPP

#include <unordered_map>
#include <vector>
#include <cstdint>

extern "C" {
#include "E2AP-PDU.h"
#include "OCTET_STRING.h"
}

typedef void (*SubscriptionCallback)(E2AP_PDU_t*);

class E2Sim;
class E2Sim {

private:

  std::unordered_map<long, OCTET_STRING_t*> ran_functions_registered;
  std::unordered_map<long, SubscriptionCallback> subscription_callbacks;

  // SCTP client source port to bind (0 => historic RIC_SCTP_SRC_PORT default).
  // Set per-instance by ns-3 so multiple embedded E2 agents don't collide.
  int m_client_src_port = 0;

  // GlobalE2node gNB-ID value advertised in the E2 Setup, and the cell ids this
  // node serves. Set per-instance by ns-3 (oran-interface E2Termination) so every
  // embedded mmWave gNB advertises a UNIQUE id (else E2MGR coalesces them all into
  // one ranName) plus its served cell(s), letting a Traffic-Steering xApp build its
  // cell->node map. Default keeps historic single-gNB parity.
  uint32_t m_gnb_id_value = 0x16B8CEF1;
  std::vector<uint8_t> m_served_cells;

  void wait_for_sctp_data();

public:

  void set_client_src_port(int port) { m_client_src_port = port; }

  // Inject this E2 node's advertised gNB-ID value + served cell ids (see members).
  void set_e2node_identity(uint32_t gnb_id_value, std::vector<uint8_t> served_cells) {
    m_gnb_id_value = gnb_id_value;
    m_served_cells = served_cells;
  }


  std::unordered_map<long, OCTET_STRING_t*> getRegistered_ran_functions();

  void generate_e2apv1_subscription_response_success(E2AP_PDU *e2ap_pdu, long reqActionIdsAccepted[], long reqActionIdsRejected[], int accept_size, int reject_size, long reqRequestorId, long reqInstanceId);

  void generate_e2apv1_indication_request_parameterized(E2AP_PDU *e2ap_pdu, long requestorId, long instanceId, long ranFunctionId, long actionId, long seqNum, uint8_t *ind_header_buf, int header_length, uint8_t *ind_message_buf, int message_length);  

  SubscriptionCallback get_subscription_callback(long func_id);
  
  void register_e2sm(long func_id, OCTET_STRING_t* ostr);

  void register_subscription_callback(long func_id, SubscriptionCallback cb);
  
  void encode_and_send_sctp_data(E2AP_PDU_t* pdu);

  int run_loop(int argc, char* argv[]);

};

#endif
