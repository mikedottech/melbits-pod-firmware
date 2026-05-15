/*
  CHEAT SHEET:
  avdd [1|0]
   -> OK
  led [r|g|b|n|s|e|w] [1|0]
   -> OK
  temp
   -> (analog reading)
  light
   -> (analog reading)
  motor [1|0]
   -> OK
  spk [1|0]
   -> OK
  tone [1|0]
   -> OK
  batt
   -> 3.42 V
  acc
   -> Requesting chip ID ...
   -> [Success!|Failed]
  usb
   -> USB is [CONNECTED|DISCONNECTED]
  mlink
   -> MLINK = [1|0]

  >> EVENT: USB [1|0]
  >> EVENT: MLINK [1|0]
  >> EVENT: ACCELEROMETER INTERRUPT [1|2]

  seq x
  -> OK
*/

#include "testcmdparser.h"

#define INPUT_BUFFER_LEN (32)

static cmp_eventhandler_t s_eventHandler;

static cmp_activation_t cmp_decode_activationdigit(char c)
{
    uint8_t act = (c - '0');
    if(act > 2) act = 2;
    return (cmp_activation_t)act;
}

void cmp_init(cmp_eventhandler_t handler)
{
    s_eventHandler = handler;
}

static void parse_buffer(const uint8_t* inBuffer)
{
    //size_t len = strlen(inBuffer);
    size_t len = INPUT_BUFFER_LEN;
    cmp_activation_t        activation_code;
    
    // Parse command line
    if(strncmp(inBuffer, "avdd", 4) == 0)
    {
        // Digital VDD set
        if(len < 6) goto error;
        activation_code = cmp_decode_activationdigit(inBuffer[5]);
        cmp_event_t evt = 
        {
            .type = EVENTTYPE_DIGITAL_SET,
            .digitalset =
            {
                .outId = OUTID_AVDD,
                .activation = activation_code
            }
        };
        (*s_eventHandler)(evt);
    } else if(strncmp(inBuffer, "led", 3) == 0)
    {
        // Digital LED set
        if(len < 7) goto error;
        activation_code = cmp_decode_activationdigit(inBuffer[6]);
        cmp_outID_t out;

        if(inBuffer[4] == 'r') { out = OUTID_R; }
        else if(inBuffer[4] == 'g') { out = OUTID_G; }
        else if(inBuffer[4] == 'b') { out = OUTID_B; }
        else if(inBuffer[4] == 'n') { out = OUTID_N; }
        else if(inBuffer[4] == 's') { out = OUTID_S; }
        else if(inBuffer[4] == 'e') { out = OUTID_E; }
        else if(inBuffer[4] == 'w') { out = OUTID_W; }
        else goto error;

        cmp_event_t evt = 
        {
            .type = EVENTTYPE_DIGITAL_SET,
            .digitalset =
            {
                .outId = out,
                .activation = activation_code
            }
        };
        (*s_eventHandler)(evt);
    } else if(strncmp(inBuffer, "temp", 4) == 0)
    {
        // Temperature reading
        cmp_event_t evt = 
        {
            .type = EVENTTYPE_ANALOG_READ,
            .analogread =
            {
                .ainID = AINID_TEMP
            }
        };
        (*s_eventHandler)(evt);
    } else if(strncmp(inBuffer, "light", 5) == 0)
    {
        // Light reading
        cmp_event_t evt = 
        {
            .type = EVENTTYPE_ANALOG_READ,
            .analogread =
            {
                .ainID = AINID_LIGHT
            }
        };
        (*s_eventHandler)(evt);
    } else if(strncmp(inBuffer, "motor", 5) == 0)
    {
        // Motor on/off
        if(len < 7) goto error;
        activation_code = cmp_decode_activationdigit(inBuffer[6]);
        cmp_event_t evt = 
        {
            .type = EVENTTYPE_DIGITAL_SET,
            .digitalset =
            {
                .outId = OUTID_VIBRATION,
                .activation = activation_code
            }
        };
        (*s_eventHandler)(evt);
    } else if(strncmp(inBuffer, "spk", 3) == 0)
    {
        // Digital SPK set
        if(len < 5) goto error;
        activation_code = cmp_decode_activationdigit(inBuffer[4]);
        cmp_event_t evt = 
        {
            .type = EVENTTYPE_DIGITAL_SET,
            .digitalset =
            {
                .outId = OUTID_SPK,
                .activation = activation_code
            }
        };
        (*s_eventHandler)(evt);
    } else if(strncmp(inBuffer, "tone", 4) == 0)
    {
        // Play tone
        if(len < 6) goto error;
        activation_code = cmp_decode_activationdigit(inBuffer[5]);
        cmp_event_t evt = 
        {
            .type = EVENTTYPE_TONE                    
        };
        (*s_eventHandler)(evt);
    } else if(strncmp(inBuffer, "batt", 4) == 0)
    {
        // Bettery reading                                        
        cmp_event_t evt = 
        {
            .type = EVENTTYPE_ANALOG_READ,
            .analogread =
            {
                .ainID = AINID_BATT
            }
        };
        (*s_eventHandler)(evt);
    } else if(strncmp(inBuffer, "acc2", 4) == 0)
    {
        // Accelerometer test
        cmp_event_t evt = 
        {
            .type = EVENTTYPE_ACCELTEST2,
        };
        (*s_eventHandler)(evt);
    } else if(strncmp(inBuffer, "acc3", 4) == 0)
    {
        // Accelerometer test
        cmp_event_t evt = 
        {
            .type = EVENTTYPE_ACCELTEST3,
        };
        (*s_eventHandler)(evt);
    } else if(strncmp(inBuffer, "acc", 3) == 0)
    {
        // Accelerometer test
        cmp_event_t evt = 
        {
            .type = EVENTTYPE_ACCELTEST,
        };
        (*s_eventHandler)(evt);
    } else if(strncmp(inBuffer, "i2c", 3) == 0)
    {
        // I2C operation
        char rd = inBuffer[4];

        cmp_event_t evt = 
        {
            .type = EVENTTYPE_I2C,
        };

        if(rd == 'w')
        {
            evt.i2c.isRead = 0;
            uint32_t addr, val;
            uint32_t res = sscanf(&inBuffer[6], "%x %x", &addr, &val);
            if(res == 2)
            {
                evt.i2c.addr = (uint8_t)addr;
                evt.i2c.value = (uint8_t)val;
            }
        } else {
            evt.i2c.isRead = 1;
            uint32_t addr;
            uint32_t res = sscanf(&inBuffer[6], "%x", &addr);
            if(res == 1)
            {
                evt.i2c.addr = (uint8_t)addr;                
            }
        }

        (*s_eventHandler)(evt);
    } else if(strncmp(inBuffer, "usb", 3) == 0)
    {
        // USB reading                    
        cmp_event_t evt = 
        {
            .type = EVENTTYPE_DIGITAL_READ,
            .digitalread =
            {
                .inId = INID_USB
            }
        };
        (*s_eventHandler)(evt);
    } else if(strncmp(inBuffer, "mlink", 5) == 0)
    {
        // MLINK reading
        cmp_event_t evt = 
        {
            .type = EVENTTYPE_DIGITAL_READ,
            .digitalread =
            {
                .inId = INID_MLINK
            }
        };
        (*s_eventHandler)(evt);
    } else if(strncmp(inBuffer, "seq", 3) == 0)
    {
        // Trigger sequence
        uint8_t seqID = inBuffer[4] - '0';
        cmp_event_t evt = 
        {
            .type = EVENTTYPE_SEQ,
            .seqtrigger =
            {
                .seqID = seqID
            }
        };
        (*s_eventHandler)(evt);
    }

    return;

error:
    {
        cmp_event_t evt = 
        {
            .type = EVENTTYPE_ERROR,
        };
        (*s_eventHandler)(evt);
    }   
}

void cmp_feed_data(const uint8_t* pData, uint16_t size)
{
    static char inBuffer[INPUT_BUFFER_LEN] = {0};  
    static uint8_t index = 0;    
    
    for(uint16_t i = 0; i < size; ++i)
    {
        char c = pData[i];

        if ((c == '\n') ||
          (c == '\r') ||
          (c == ';') ||
          (index >= INPUT_BUFFER_LEN))
        {               
            if (index > 1)
            {
                index = 0;
                inBuffer[INPUT_BUFFER_LEN - 1] = 0;
                parse_buffer(inBuffer);
                memset(inBuffer, 0, INPUT_BUFFER_LEN);                
            }
        } else {
            inBuffer[index++] = pData[i];
        }
    }
}
