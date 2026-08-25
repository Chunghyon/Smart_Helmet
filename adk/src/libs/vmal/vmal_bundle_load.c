/****************************************************************************
Copyright (c) 2021 Qualcomm Technologies International, Ltd.
 
FILE NAME
    vmal_bundle_load.c
 
DESCRIPTION
    VMAL wrapper around OperatorBundleLoad() 
 
NOTES
 
*/
 
#include <vmal.h>
#include <operator.h>
BundleID VmalOperatorBundleLoad(FILE_INDEX index, capability_bundle_processor_availability_t capability)
{
    return OperatorBundleLoad(index, capability);
}