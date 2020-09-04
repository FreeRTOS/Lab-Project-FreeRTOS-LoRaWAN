#ifndef __PIN_NAME_IOE_H__
#define __PIN_NAME_IOE_H__

#ifdef __cplusplus
extern "C"
{
#endif

/* In attempt to leave loramac stack untouched, we don't want to alter its gpio.h.
 * So We install this DUMMY pin for IOE that can not be used
 */
#define IOE_PINS PIN_DNE

#ifdef __cplusplus
}
#endif

#endif // __PIN_NAME_IOE_H__
