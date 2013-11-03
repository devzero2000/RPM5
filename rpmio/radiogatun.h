/* RadioGatún reference code
 * Public domain
 * For more information on RadioGatún, please refer to 
 * http://radiogatun.noekeon.org/
*/
#ifndef _RADIOGATUN_H_
#define _RADIOGATUN_H_

#ifdef __cplusplus
extern "C" {
#endif

void RadioGatun32_FastIterate(uint32_t *a, uint32_t *b, const uint32_t * in, unsigned int Nr13Blocks);
void RadioGatun64_FastIterate(uint64_t *a, uint64_t *b, const uint64_t * in, unsigned int Nr13Blocks);

#ifdef __cplusplus
}
#endif

#endif	/* _RADIOGATUN_H */
