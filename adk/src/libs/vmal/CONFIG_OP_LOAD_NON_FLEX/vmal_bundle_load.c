/*******************************************************************************
Copyright (c) 2021 Qualcomm Technologies International, Ltd.
 
FILE NAME
    vmal_transform.c
 
DESCRIPTION
    VMAL wrapper around OperatorBundleLoad()
*/

#include <vmal.h>
#include <operator.h>
 
/*!
    \brief  Maps capability_bundle_processor_availability_t values from QCC515x and 
            later values.
 
    \param capability Specifies the value to map from.
 
    \return The value capability mapped to.
*/
static capability_bundle_processor_availability_t capabilityBundleProcessorAvailabilityQCC514xMap(capability_bundle_processor_availability_t capability)
{
    switch(capability)
    {
        case capability_load_to_p0_use_on_p0_only:
        case capability_load_to_p0_use_on_both:
            return capability_load_to_p0_use_on_p0_only;
        case capability_load_to_p1_use_on_p1_only:
        case capability_load_to_p1_use_on_both:
        default:
            return capability;
    }
}
BundleID VmalOperatorBundleLoad(FILE_INDEX index, capability_bundle_processor_availability_t capability)
{
    return OperatorBundleLoad(index, capabilityBundleProcessorAvailabilityQCC514xMap(capability));
}