/* chi.h: CHI hash */

/*
THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE AND AGAINST
INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef _CHI_H
#define _CHI_H

#define	BitSequence	chi_BitSequence
#define	DataLength	chi_DataLength
#define	hashState	chi_hashState
#define	HashReturn	int

#define	Init		chi_Init
#define	Update		chi_Update
#define	Final		chi_Final
#define	Hash		chi_Hash

#include <stdint.h>

/*
 * BitSequence type for Message Data
 */
typedef unsigned char BitSequence;

/*
 * DataLength type big, for big data lengths!.
 */
typedef unsigned long long DataLength;

#define _256_STATE_N    6       /* The number of words in the internal state
                                 * for the 256 bit hash.
                                 */
#define _512_STATE_N    9       /* The number of words in the internal state
                                 * for the 512 bit hash.
                                 */
/*
 * Structure that stores the hash state.
 */
typedef struct {
    union {
        uint64_t small[_256_STATE_N];
        uint64_t large[_512_STATE_N];
        uint32_t small32[2*_256_STATE_N];
        uint32_t large32[2*_512_STATE_N];
    } hs_State;                     /* Contents of internal state             */

    int         hs_HashBitLen;      /* Length of the Hash.  Passed into Init. */
    int         hs_MessageLen;      /* Length of message block in bits.  This
                                     * is set in Init depending on hashbitlen
                                     * to _256_MSG_LENGTH for 224,256 bit hash
                                     * and _512_MSG_LENGTH for 384,512 bit
                                     * hash.
                                     */
    uint64_t    hs_TotalLenLow;     /* Total length of message in bits.  Does
                                     * not include length of padding.
                                     */
    uint64_t    hs_TotalLenHigh;    /* For exceedingly long messages.
                                     */
    uint32_t    hs_DataLen;         /* Number of unprocess bits in
                                     * hs_DataBuffer.
                                     */
    BitSequence hs_DataBuffer[128]; /* Buffer for accumulating message.       */
} hashState;

/*
 * External Interface
 */
HashReturn Init(hashState*, int); 
HashReturn Update(hashState*, const BitSequence*, DataLength); 
HashReturn Final(hashState*, BitSequence*); 
HashReturn Hash(int, const BitSequence*, DataLength, BitSequence*);

/* Impedance match bytes -> bits length. */
static inline
int _chi_Update(void * param, const void * _data, size_t _len)
{
    return Update(param, _data, (DataLength)(8 * _len));
}

#endif /* _CHI_H */
