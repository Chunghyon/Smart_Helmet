/*!
\copyright  Copyright (c) 2021 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera chain configuration routines common to all A2DP Source chains.
*/

#ifndef KYMERA_A2DP_SOURCE_H
#define KYMERA_A2DP_SOURCE_H

#if defined(INCLUDE_A2DP_USB_SOURCE) || defined(INCLUDE_A2DP_ANALOG_SOURCE)

#include <a2dp.h>
#include <chain.h>

/*!
    \brief Initialise this module. Must be called before any other function.
*/
void kymeraA2dpSource_Init(void);

/*!
    \brief Configure the TTP buffer with required latencies / sizes.

    All A2DP source chains have an operator that sets the Time-To-Play latency
    for the chain, outputting to a buffer located somewhere before the encoder.
    This operator needs configuring with the correct TTP latencies and buffer size.
*/
void kymeraA2dpSource_ConfigureTtpBufferParams(Operator operator, uint32 min_latency_ms,
                                               uint32 max_latency_ms, uint32 target_latency_ms);

/*!
    \brief Configure the encoder selected during A2DP codec negotiation.

    Configure the appropriate encoder, with the parameters negotiated during
    A2DP codec selection. Also sets the encoder output buffer size as required.
    The A2DP parametes are expected to have already been set via a call to
    Kymera_SetA2dpOutputParams(), before calling this function.
*/
void kymeraA2dpSource_ConfigureEncoder(void);

/*!
    \brief Configure the switched passthrough operator at the end of the chain.

    All A2DP source chains end with a switched passthrough consumer, due to the
    transient nature of the A2DP media channel the chain outputs to (there could
    for example be a link loss whilst the chain is running).
*/
void kymeraA2dpSource_ConfigureSwitchedPassthrough(void);

/*!
    \brief Configure the packetiser for A2DP source chains.
    \return TRUE if packetiser configured successfully, FALSE otherwise.
*/
bool kymeraA2dpSource_ConfigurePacketiser(void);

/*!
    \brief Start the chain, performing any actions common to all source chains.
*/
void kymeraA2dpSource_StartChain(void);

/*!
    \brief Stop the chain, performing any actions common to all source chains.
*/
void kymeraA2dpSource_StopChain(void);

/*!
    \brief Disconnect and destroy the common parts of the A2DP source chain.

    Hardware-specific inputs should already have been disconnected from the
    chain before calling this function (as it will destroy the chain).
*/
void kymeraA2dpSource_DestroyChain(void);


#endif /* INCLUDE_A2DP_USB_SOURCE || INCLUDE_A2DP_ANALOG_SOURCE */

#endif /* KYMERA_A2DP_SOURCE_H */
