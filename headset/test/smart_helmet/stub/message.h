/*!
\file       message.h
\brief      Host-test stub for the QCC ADK message subsystem.

NOTE on declaration order: TaskData holds a handler taking (Task, MessageId,
Message), so those three typedefs must come first.
*/
#ifndef SH_TEST_MESSAGE_H
#define SH_TEST_MESSAGE_H

#include <csrtypes.h>

typedef struct sh_test_task_data *Task;
typedef uint16 MessageId;
typedef const void *Message;

typedef struct sh_test_task_data
{
    void (*handler)(Task, MessageId, Message);
} TaskData;

#define MESSAGE_ADC_RESULT (0x40u)

typedef struct
{
    uint16 adc_source;
    uint16 reading;
} MessageAdcResult;

static inline void MessageCancelAll(Task t, MessageId id)
{
    UNUSED(t); UNUSED(id);
}

static inline void MessageSendLater(Task t, MessageId id, void *m, uint32 delay)
{
    UNUSED(t); UNUSED(id); UNUSED(m); UNUSED(delay);
}

static inline void MessageSend(Task t, MessageId id, void *m)
{
    UNUSED(t); UNUSED(id); UNUSED(m);
}

#endif /* SH_TEST_MESSAGE_H */
