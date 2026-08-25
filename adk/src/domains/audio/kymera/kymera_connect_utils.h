/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Helper definitions for audio connections
*/

#ifndef KYMERA_CONNECT_UTILS_H_
#define KYMERA_CONNECT_UTILS_H_

/**
 *  \brief Connect if both Source and Sink are valid.
 *  \param source The Source data will be taken from.
 *  \param sink The Sink data will be written to.
 *  \note In the case of connection failuar, it will panics the application.
 * */
void Kymera_ConnectIfValid(Source source, Sink sink);

/**
 *  \brief Break any existing automatic connection involving the source *or* sink.
 *   Source or sink may be NULL.
 *  \param source The source which needs to be disconnected.
 *  \param sink The sink which needs to be disconnected.
 * */
void Kymera_DisconnectIfValid(Source source, Sink sink);

/*  \brief Connect audio output chain endpoints to appropriate hardware outputs
    \param left source of Left output channel
    \param right source of Right output channel if stereo output supported
    \param output_sample_rate The output sample rate to be set
*/
void Kymera_ConnectOutputSource(Source left, Source right, uint32 output_sample_rate);

#endif /* KYMERA_CONNECT_UTILS_H_ */
