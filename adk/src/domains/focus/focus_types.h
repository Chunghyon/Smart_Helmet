/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   focus_domain Focus
\ingroup    domains
*/
#ifndef FOCUS_TYPES_H
#define FOCUS_TYPES_H

/*! @{ */

/*! \brief Focus type associated with a particular device or source. */
typedef enum
{
    focus_none,         /*!< No focus */
    focus_background,   /*!< Background focus */
    focus_foreground    /*!< Foreground focus */

} focus_t;

/*! @} */

#endif /* FOCUS_TYPES_H */
