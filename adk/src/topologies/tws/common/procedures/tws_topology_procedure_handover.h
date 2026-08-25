/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\brief      Generic interface to TWS Topology procedure handling for handover.
*/

#ifndef TWS_TOPOLOGY_PROCEDURE_HANDOVER_H
#define TWS_TOPOLOGY_PROCEDURE_HANDOVER_H

#include "script_engine.h"

/*! Script to prepare for handover */
extern const procedure_script_t dynamic_handover_prepare_script;

/*! Script to perform handover */
extern const procedure_script_t dynamic_handover_script;

/*! Script to undo the steps performed during prepare for handover, e.g. if
handover is cancelled */
extern const procedure_script_t dynamic_handover_undo_prepare_script;

#endif /* TWS_TOPOLOGY_PROCEDURE_HANDOVER_H */

