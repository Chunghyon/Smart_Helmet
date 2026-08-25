/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   ui_domain UI
\ingroup    domains
\brief      Protected methods which have scope of visibility only within the UI domain.
*/
#ifndef UI_PROTECTED_H_
#define UI_PROTECTED_H_

#include <csrtypes.h>

#include "ui.h"

const ui_config_table_content_t * Ui_GetConstUiConfigTable(unsigned * number_of_elements);

#endif /* UI_PROTECTED_H_ */


