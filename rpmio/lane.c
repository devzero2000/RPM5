/* laneref.c   v1.1   October 2008
 * Reference ANSI C implementation of LANE
 *
 * Based on the AES reference implementation of Paulo Barreto and Vincent Rijmen.
 *
 * Author: Nicky Mouha
 *
 * This code is placed in the public domain.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lane.h"

enum { SUCCESS        = 0, /* Execution successful */
       FAIL           = 1, /* Unspecified failure occurred */
       BAD_HASHBITLEN = 2, /* Bad length in bits of the hash value */
       BAD_DATABITLEN = 3  /* Incomplete byte before last call to Update() */
};

typedef uint8_t aes_state[4][4]; /* a 4x4 byte array containing the AES state */

/* Swap two bytes. */
void swap_bytes(uint8_t *a, uint8_t *b);

/* Select a byte from a 32-bit word, most significant byte has index 0. */
uint8_t select_byte_32(uint32_t word, int idx);

/* Select a byte from a 64-bit word, most significant byte has index 0. */
uint8_t select_byte_64(DataLength word, int idx);

/* Multiply an element of GF(2^m) by 3, needed for MixColumns256() and MixColumns512(). */
uint8_t mul2(uint8_t a);

/* Multiply an element of GF(2^m) by 3, needed for MixColumns256() and MixColumns512(). */
uint8_t mul3(uint8_t a);

/* ===== "tables.h" */
static const uint8_t S[256] = {
0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5, 0x30, 0x01, 0x67, 0x2B, 0xFE, 0xD7, 0xAB, 0x76, 
0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0, 0xAD, 0xD4, 0xA2, 0xAF, 0x9C, 0xA4, 0x72, 0xC0, 
0xB7, 0xFD, 0x93, 0x26, 0x36, 0x3F, 0xF7, 0xCC, 0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15, 
0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A, 0x07, 0x12, 0x80, 0xE2, 0xEB, 0x27, 0xB2, 0x75, 
0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E, 0x5A, 0xA0, 0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84, 
0x53, 0xD1, 0x00, 0xED, 0x20, 0xFC, 0xB1, 0x5B, 0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF, 
0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85, 0x45, 0xF9, 0x02, 0x7F, 0x50, 0x3C, 0x9F, 0xA8, 
0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5, 0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2, 
0xCD, 0x0C, 0x13, 0xEC, 0x5F, 0x97, 0x44, 0x17, 0xC4, 0xA7, 0x7E, 0x3D, 0x64, 0x5D, 0x19, 0x73, 
0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88, 0x46, 0xEE, 0xB8, 0x14, 0xDE, 0x5E, 0x0B, 0xDB, 
0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C, 0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79, 
0xE7, 0xC8, 0x37, 0x6D, 0x8D, 0xD5, 0x4E, 0xA9, 0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08, 
0xBA, 0x78, 0x25, 0x2E, 0x1C, 0xA6, 0xB4, 0xC6, 0xE8, 0xDD, 0x74, 0x1F, 0x4B, 0xBD, 0x8B, 0x8A, 
0x70, 0x3E, 0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E, 0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E, 
0xE1, 0xF8, 0x98, 0x11, 0x69, 0xD9, 0x8E, 0x94, 0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF, 
0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68, 0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16, 
};

/* Constants k are generated using an LFSR.
 *
 * Code to generate these constants:
 *   int i;
 *   uint32_t k[768];
 *
 *   k[0] = 0x07FC703D;
 *   for (i=1; i!=768; i++) {
 *     k[i] = k[i-1] >> 1;
 *     if (k[i-1] & 0x00000001) {
 *       k[i] ^= 0xD0000001;
 *     }
 *   }
 */
static const uint32_t k[768] = {
0x07FC703D, 0xD3FE381F, 0xB9FF1C0E, 0x5CFF8E07, 0xFE7FC702, 0x7F3FE381, 0xEF9FF1C1, 0xA7CFF8E1, 
0x83E7FC71, 0x91F3FE39, 0x98F9FF1D, 0x9C7CFF8F, 0x9E3E7FC6, 0x4F1F3FE3, 0xF78F9FF0, 0x7BC7CFF8, 
0x3DE3E7FC, 0x1EF1F3FE, 0x0F78F9FF, 0xD7BC7CFE, 0x6BDE3E7F, 0xE5EF1F3E, 0x72F78F9F, 0xE97BC7CE, 
0x74BDE3E7, 0xEA5EF1F2, 0x752F78F9, 0xEA97BC7D, 0xA54BDE3F, 0x82A5EF1E, 0x4152F78F, 0xF0A97BC6, 
0x7854BDE3, 0xEC2A5EF0, 0x76152F78, 0x3B0A97BC, 0x1D854BDE, 0x0EC2A5EF, 0xD76152F6, 0x6BB0A97B, 
0xE5D854BC, 0x72EC2A5E, 0x3976152F, 0xCCBB0A96, 0x665D854B, 0xE32EC2A4, 0x71976152, 0x38CBB0A9, 
0xCC65D855, 0xB632EC2B, 0x8B197614, 0x458CBB0A, 0x22C65D85, 0xC1632EC3, 0xB0B19760, 0x5858CBB0, 
0x2C2C65D8, 0x161632EC, 0x0B0B1976, 0x05858CBB, 0xD2C2C65C, 0x6961632E, 0x34B0B197, 0xCA5858CA, 
0x652C2C65, 0xE2961633, 0xA14B0B18, 0x50A5858C, 0x2852C2C6, 0x14296163, 0xDA14B0B0, 0x6D0A5858, 
0x36852C2C, 0x1B429616, 0x0DA14B0B, 0xD6D0A584, 0x6B6852C2, 0x35B42961, 0xCADA14B1, 0xB56D0A59, 
0x8AB6852D, 0x955B4297, 0x9AADA14A, 0x4D56D0A5, 0xF6AB6853, 0xAB55B428, 0x55AADA14, 0x2AD56D0A, 
0x156AB685, 0xDAB55B43, 0xBD5AADA0, 0x5EAD56D0, 0x2F56AB68, 0x17AB55B4, 0x0BD5AADA, 0x05EAD56D, 
0xD2F56AB7, 0xB97AB55A, 0x5CBD5AAD, 0xFE5EAD57, 0xAF2F56AA, 0x5797AB55, 0xFBCBD5AB, 0xADE5EAD4, 
0x56F2F56A, 0x2B797AB5, 0xC5BCBD5B, 0xB2DE5EAC, 0x596F2F56, 0x2CB797AB, 0xC65BCBD4, 0x632DE5EA, 
0x3196F2F5, 0xC8CB797B, 0xB465BCBC, 0x5A32DE5E, 0x2D196F2F, 0xC68CB796, 0x63465BCB, 0xE1A32DE4, 
0x70D196F2, 0x3868CB79, 0xCC3465BD, 0xB61A32DF, 0x8B0D196E, 0x45868CB7, 0xF2C3465A, 0x7961A32D, 
0xECB0D197, 0xA65868CA, 0x532C3465, 0xF9961A33, 0xACCB0D18, 0x5665868C, 0x2B32C346, 0x159961A3, 
0xDACCB0D0, 0x6D665868, 0x36B32C34, 0x1B59961A, 0x0DACCB0D, 0xD6D66587, 0xBB6B32C2, 0x5DB59961, 
0xFEDACCB1, 0xAF6D6659, 0x87B6B32D, 0x93DB5997, 0x99EDACCA, 0x4CF6D665, 0xF67B6B33, 0xAB3DB598, 
0x559EDACC, 0x2ACF6D66, 0x1567B6B3, 0xDAB3DB58, 0x6D59EDAC, 0x36ACF6D6, 0x1B567B6B, 0xDDAB3DB4, 
0x6ED59EDA, 0x376ACF6D, 0xCBB567B7, 0xB5DAB3DA, 0x5AED59ED, 0xFD76ACF7, 0xAEBB567A, 0x575DAB3D, 
0xFBAED59F, 0xADD76ACE, 0x56EBB567, 0xFB75DAB2, 0x7DBAED59, 0xEEDD76AD, 0xA76EBB57, 0x83B75DAA, 
0x41DBAED5, 0xF0EDD76B, 0xA876EBB4, 0x543B75DA, 0x2A1DBAED, 0xC50EDD77, 0xB2876EBA, 0x5943B75D, 
0xFCA1DBAF, 0xAE50EDD6, 0x572876EB, 0xFB943B74, 0x7DCA1DBA, 0x3EE50EDD, 0xCF72876F, 0xB7B943B6, 
0x5BDCA1DB, 0xFDEE50EC, 0x7EF72876, 0x3F7B943B, 0xCFBDCA1C, 0x67DEE50E, 0x33EF7287, 0xC9F7B942, 
0x64FBDCA1, 0xE27DEE51, 0xA13EF729, 0x809F7B95, 0x904FBDCB, 0x9827DEE4, 0x4C13EF72, 0x2609F7B9, 
0xC304FBDD, 0xB1827DEF, 0x88C13EF6, 0x44609F7B, 0xF2304FBC, 0x791827DE, 0x3C8C13EF, 0xCE4609F6, 
0x672304FB, 0xE391827C, 0x71C8C13E, 0x38E4609F, 0xCC72304E, 0x66391827, 0xE31C8C12, 0x718E4609, 
0xE8C72305, 0xA4639183, 0x8231C8C0, 0x4118E460, 0x208C7230, 0x10463918, 0x08231C8C, 0x04118E46, 
0x0208C723, 0xD1046390, 0x688231C8, 0x344118E4, 0x1A208C72, 0x0D104639, 0xD688231D, 0xBB44118F, 
0x8DA208C6, 0x46D10463, 0xF3688230, 0x79B44118, 0x3CDA208C, 0x1E6D1046, 0x0F368823, 0xD79B4410, 
0x6BCDA208, 0x35E6D104, 0x1AF36882, 0x0D79B441, 0xD6BCDA21, 0xBB5E6D11, 0x8DAF3689, 0x96D79B45, 
0x9B6BCDA3, 0x9DB5E6D0, 0x4EDAF368, 0x276D79B4, 0x13B6BCDA, 0x09DB5E6D, 0xD4EDAF37, 0xBA76D79A, 
0x5D3B6BCD, 0xFE9DB5E7, 0xAF4EDAF2, 0x57A76D79, 0xFBD3B6BD, 0xADE9DB5F, 0x86F4EDAE, 0x437A76D7, 
0xF1BD3B6A, 0x78DE9DB5, 0xEC6F4EDB, 0xA637A76C, 0x531BD3B6, 0x298DE9DB, 0xC4C6F4EC, 0x62637A76, 
0x3131BD3B, 0xC898DE9C, 0x644C6F4E, 0x322637A7, 0xC9131BD2, 0x64898DE9, 0xE244C6F5, 0xA122637B, 
0x809131BC, 0x404898DE, 0x20244C6F, 0xC0122636, 0x6009131B, 0xE004898C, 0x700244C6, 0x38012263, 
0xCC009130, 0x66004898, 0x3300244C, 0x19801226, 0x0CC00913, 0xD6600488, 0x6B300244, 0x35980122, 
0x1ACC0091, 0xDD660049, 0xBEB30025, 0x8F598013, 0x97ACC008, 0x4BD66004, 0x25EB3002, 0x12F59801, 
0xD97ACC01, 0xBCBD6601, 0x8E5EB301, 0x972F5981, 0x9B97ACC1, 0x9DCBD661, 0x9EE5EB31, 0x9F72F599, 
0x9FB97ACD, 0x9FDCBD67, 0x9FEE5EB2, 0x4FF72F59, 0xF7FB97AD, 0xABFDCBD7, 0x85FEE5EA, 0x42FF72F5, 
0xF17FB97B, 0xA8BFDCBC, 0x545FEE5E, 0x2A2FF72F, 0xC517FB96, 0x628BFDCB, 0xE145FEE4, 0x70A2FF72, 
0x38517FB9, 0xCC28BFDD, 0xB6145FEF, 0x8B0A2FF6, 0x458517FB, 0xF2C28BFC, 0x796145FE, 0x3CB0A2FF, 
0xCE58517E, 0x672C28BF, 0xE396145E, 0x71CB0A2F, 0xE8E58516, 0x7472C28B, 0xEA396144, 0x751CB0A2, 
0x3A8E5851, 0xCD472C29, 0xB6A39615, 0x8B51CB0B, 0x95A8E584, 0x4AD472C2, 0x256A3961, 0xC2B51CB1, 
0xB15A8E59, 0x88AD472D, 0x9456A397, 0x9A2B51CA, 0x4D15A8E5, 0xF68AD473, 0xAB456A38, 0x55A2B51C, 
0x2AD15A8E, 0x1568AD47, 0xDAB456A2, 0x6D5A2B51, 0xE6AD15A9, 0xA3568AD5, 0x81AB456B, 0x90D5A2B4, 
0x486AD15A, 0x243568AD, 0xC21AB457, 0xB10D5A2A, 0x5886AD15, 0xFC43568B, 0xAE21AB44, 0x5710D5A2, 
0x2B886AD1, 0xC5C43569, 0xB2E21AB5, 0x89710D5B, 0x94B886AC, 0x4A5C4356, 0x252E21AB, 0xC29710D4, 
0x614B886A, 0x30A5C435, 0xC852E21B, 0xB429710C, 0x5A14B886, 0x2D0A5C43, 0xC6852E20, 0x63429710, 
0x31A14B88, 0x18D0A5C4, 0x0C6852E2, 0x06342971, 0xD31A14B9, 0xB98D0A5D, 0x8CC6852F, 0x96634296, 
0x4B31A14B, 0xF598D0A4, 0x7ACC6852, 0x3D663429, 0xCEB31A15, 0xB7598D0B, 0x8BACC684, 0x45D66342, 
0x22EB31A1, 0xC17598D1, 0xB0BACC69, 0x885D6635, 0x942EB31B, 0x9A17598C, 0x4D0BACC6, 0x2685D663, 
0xC342EB30, 0x61A17598, 0x30D0BACC, 0x18685D66, 0x0C342EB3, 0xD61A1758, 0x6B0D0BAC, 0x358685D6, 
0x1AC342EB, 0xDD61A174, 0x6EB0D0BA, 0x3758685D, 0xCBAC342F, 0xB5D61A16, 0x5AEB0D0B, 0xFD758684, 
0x7EBAC342, 0x3F5D61A1, 0xCFAEB0D1, 0xB7D75869, 0x8BEBAC35, 0x95F5D61B, 0x9AFAEB0C, 0x4D7D7586, 
0x26BEBAC3, 0xC35F5D60, 0x61AFAEB0, 0x30D7D758, 0x186BEBAC, 0x0C35F5D6, 0x061AFAEB, 0xD30D7D74, 
0x6986BEBA, 0x34C35F5D, 0xCA61AFAF, 0xB530D7D6, 0x5A986BEB, 0xFD4C35F4, 0x7EA61AFA, 0x3F530D7D, 
0xCFA986BF, 0xB7D4C35E, 0x5BEA61AF, 0xFDF530D6, 0x7EFA986B, 0xEF7D4C34, 0x77BEA61A, 0x3BDF530D, 
0xCDEFA987, 0xB6F7D4C2, 0x5B7BEA61, 0xFDBDF531, 0xAEDEFA99, 0x876F7D4D, 0x93B7BEA7, 0x99DBDF52, 
0x4CEDEFA9, 0xF676F7D5, 0xAB3B7BEB, 0x859DBDF4, 0x42CEDEFA, 0x21676F7D, 0xC0B3B7BF, 0xB059DBDE, 
0x582CEDEF, 0xFC1676F6, 0x7E0B3B7B, 0xEF059DBC, 0x7782CEDE, 0x3BC1676F, 0xCDE0B3B6, 0x66F059DB, 
0xE3782CEC, 0x71BC1676, 0x38DE0B3B, 0xCC6F059C, 0x663782CE, 0x331BC167, 0xC98DE0B2, 0x64C6F059, 
0xE263782D, 0xA131BC17, 0x8098DE0A, 0x404C6F05, 0xF0263783, 0xA8131BC0, 0x54098DE0, 0x2A04C6F0, 
0x15026378, 0x0A8131BC, 0x054098DE, 0x02A04C6F, 0xD1502636, 0x68A8131B, 0xE454098C, 0x722A04C6, 
0x39150263, 0xCC8A8130, 0x66454098, 0x3322A04C, 0x19915026, 0x0CC8A813, 0xD6645408, 0x6B322A04, 
0x35991502, 0x1ACC8A81, 0xDD664541, 0xBEB322A1, 0x8F599151, 0x97ACC8A9, 0x9BD66455, 0x9DEB322B, 
0x9EF59914, 0x4F7ACC8A, 0x27BD6645, 0xC3DEB323, 0xB1EF5990, 0x58F7ACC8, 0x2C7BD664, 0x163DEB32, 
0x0B1EF599, 0xD58F7ACD, 0xBAC7BD67, 0x8D63DEB2, 0x46B1EF59, 0xF358F7AD, 0xA9AC7BD7, 0x84D63DEA, 
0x426B1EF5, 0xF1358F7B, 0xA89AC7BC, 0x544D63DE, 0x2A26B1EF, 0xC51358F6, 0x6289AC7B, 0xE144D63C, 
0x70A26B1E, 0x3851358F, 0xCC289AC6, 0x66144D63, 0xE30A26B0, 0x71851358, 0x38C289AC, 0x1C6144D6, 
0x0E30A26B, 0xD7185134, 0x6B8C289A, 0x35C6144D, 0xCAE30A27, 0xB5718512, 0x5AB8C289, 0xFD5C6145, 
0xAEAE30A3, 0x87571850, 0x43AB8C28, 0x21D5C614, 0x10EAE30A, 0x08757185, 0xD43AB8C3, 0xBA1D5C60, 
0x5D0EAE30, 0x2E875718, 0x1743AB8C, 0x0BA1D5C6, 0x05D0EAE3, 0xD2E87570, 0x69743AB8, 0x34BA1D5C, 
0x1A5D0EAE, 0x0D2E8757, 0xD69743AA, 0x6B4BA1D5, 0xE5A5D0EB, 0xA2D2E874, 0x5169743A, 0x28B4BA1D, 
0xC45A5D0F, 0xB22D2E86, 0x59169743, 0xFC8B4BA0, 0x7E45A5D0, 0x3F22D2E8, 0x1F916974, 0x0FC8B4BA, 
0x07E45A5D, 0xD3F22D2F, 0xB9F91696, 0x5CFC8B4B, 0xFE7E45A4, 0x7F3F22D2, 0x3F9F9169, 0xCFCFC8B5, 
0xB7E7E45B, 0x8BF3F22C, 0x45F9F916, 0x22FCFC8B, 0xC17E7E44, 0x60BF3F22, 0x305F9F91, 0xC82FCFC9, 
0xB417E7E5, 0x8A0BF3F3, 0x9505F9F8, 0x4A82FCFC, 0x25417E7E, 0x12A0BF3F, 0xD9505F9E, 0x6CA82FCF, 
0xE65417E6, 0x732A0BF3, 0xE99505F8, 0x74CA82FC, 0x3A65417E, 0x1D32A0BF, 0xDE99505E, 0x6F4CA82F, 
0xE7A65416, 0x73D32A0B, 0xE9E99504, 0x74F4CA82, 0x3A7A6541, 0xCD3D32A1, 0xB69E9951, 0x8B4F4CA9, 
0x95A7A655, 0x9AD3D32B, 0x9D69E994, 0x4EB4F4CA, 0x275A7A65, 0xC3AD3D33, 0xB1D69E98, 0x58EB4F4C, 
0x2C75A7A6, 0x163AD3D3, 0xDB1D69E8, 0x6D8EB4F4, 0x36C75A7A, 0x1B63AD3D, 0xDDB1D69F, 0xBED8EB4E, 
0x5F6C75A7, 0xFFB63AD2, 0x7FDB1D69, 0xEFED8EB5, 0xA7F6C75B, 0x83FB63AC, 0x41FDB1D6, 0x20FED8EB, 
0xC07F6C74, 0x603FB63A, 0x301FDB1D, 0xC80FED8F, 0xB407F6C6, 0x5A03FB63, 0xFD01FDB0, 0x7E80FED8, 
0x3F407F6C, 0x1FA03FB6, 0x0FD01FDB, 0xD7E80FEC, 0x6BF407F6, 0x35FA03FB, 0xCAFD01FC, 0x657E80FE, 
0x32BF407F, 0xC95FA03E, 0x64AFD01F, 0xE257E80E, 0x712BF407, 0xE895FA02, 0x744AFD01, 0xEA257E81, 
0xA512BF41, 0x82895FA1, 0x9144AFD1, 0x98A257E9, 0x9C512BF5, 0x9E2895FB, 0x9F144AFC, 0x4F8A257E, 
0x27C512BF, 0xC3E2895E, 0x61F144AF, 0xE0F8A256, 0x707C512B, 0xE83E2894, 0x741F144A, 0x3A0F8A25, 
0xCD07C513, 0xB683E288, 0x5B41F144, 0x2DA0F8A2, 0x16D07C51, 0xDB683E29, 0xBDB41F15, 0x8EDA0F8B, 
0x976D07C4, 0x4BB683E2, 0x25DB41F1, 0xC2EDA0F9, 0xB176D07D, 0x88BB683F, 0x945DB41E, 0x4A2EDA0F, 
0xF5176D06, 0x7A8BB683, 0xED45DB40, 0x76A2EDA0, 0x3B5176D0, 0x1DA8BB68, 0x0ED45DB4, 0x076A2EDA, 
0x03B5176D, 0xD1DA8BB7, 0xB8ED45DA, 0x5C76A2ED, 0xFE3B5177, 0xAF1DA8BA, 0x578ED45D, 0xFBC76A2F, 
0xADE3B516, 0x56F1DA8B, 0xFB78ED44, 0x7DBC76A2, 0x3EDE3B51, 0xCF6F1DA9, 0xB7B78ED5, 0x8BDBC76B, 
0x95EDE3B4, 0x4AF6F1DA, 0x257B78ED, 0xC2BDBC77, 0xB15EDE3A, 0x58AF6F1D, 0xFC57B78F, 0xAE2BDBC6, 
0x5715EDE3, 0xFB8AF6F0, 0x7DC57B78, 0x3EE2BDBC, 0x1F715EDE, 0x0FB8AF6F, 0xD7DC57B6, 0x6BEE2BDB, 
};
/* ===== */

/* ===== "laneref256.h" */
typedef aes_state lane256_state[2];  /* a LANE-256 state consists of four AES states */

/* Adds a 32-bit constant to each column of the state.
 * This operation is performed for each part of the LANE state.
 */
void AddConstants256(int r, lane256_state a);

/* Adds part of the 64-bit counter to the state.
 * This operation is performed for each part of the LANE state.
 */
void AddCounter256(int r, lane256_state a, DataLength counter);

/* Row 0 remains unchanged, the other three rows are shifted a variable amount.
 * This operation is performed for each part of the LANE state.
 */
void ShiftRows256(lane256_state a);

/* Replace every byte of the input by the byte at that place in the nonlinear S-box.
 * This operation is performed for each part of the LANE state.
 */
void SubBytes256(lane256_state a);

/* Mix the four bytes of every column in a linear way
 * This operation is performed for each part of the LANE state.
 */
void MixColumns256(lane256_state a);

/* XOR all the elements of two LANE states together, put the result in the first one */
void XorLaneState256(lane256_state a, lane256_state b);

/* Reorder the columns of a LANE state. */
void SwapColumns256(lane256_state a);

/* Apply permutation P to the input LANE state. */
void PermuteP256(int j,               /* permutation number */
                 lane256_state a,     /* the input LANE state */
                 DataLength counter); /* the counter value used by AddCounter() */

/* Apply permutation Q to the input LANE state. */
void PermuteQ256(int j,               /* permutation number */
                 lane256_state a,     /* the input LANE state */
                 DataLength counter); /* the counter value used by AddCounter() */

/* Perform the message expansion. */
void ExpandMessage256(uint8_t *hashval,      /* storage for the intermediate hash value */
                   const uint8_t buffer[64], /* the block to be hashed */
                   lane256_state W0,       /* W0 */
                   lane256_state W1,       /* W1 */
                   lane256_state W2,       /* W2 */
                   lane256_state W3,       /* W3 */
                   lane256_state W4,       /* W4 */
                   lane256_state W5);      /* W5 */

/* Store the hash value. */
void StoreHash256(uint8_t *hashval,    /* storage for the intermediate hash value */
                  lane256_state W0); /* W0 */

/* Apply the algorithm's compression function to one block of input. */
void Lane256Transform(hashState *state, /* structure that holds hashState information */
                      const uint8_t buffer[64],    /* the block to be hashed */
                      const DataLength counter); /* counter value */

/* Adds a 32-bit constant to each column of the state.
 * This operation is performed for each part of the LANE state.
 */
void AddConstants256(int r, lane256_state a) {
  int i, j, m;

  for(m=0; m!=2; m++) { /* for each AES state of the LANE-256 state */
    for(i=0; i!=4; i++) {
      for(j=0; j!=4; j++) {
        a[m][j][i] ^= select_byte_32( k[8*r+4*m+i], j);
      }
    }
  }
}

/* Adds part of the 64-bit counter to the state
 * This operation is performed for each part of the LANE state.
 */
void AddCounter256(int r, lane256_state a, DataLength counter) {
  int j;
  for(j=0; j!=4; j++) {
    a[0][j][3] ^= select_byte_64(counter,4*(r%2)+j);
  }
}

/* Row 0 remains unchanged, the other three rows are shifted a variable amount.
 * This operation is performed for each part of the LANE state.
 */
void ShiftRows256(lane256_state a) {

  uint8_t tmp[4];
  int i, j, m;

  for(m=0; m!=2; m++) { /* for each AES state of the LANE-256 state */
    for(i=1; i!=4; i++) {
      for(j=0; j!=4; j++) tmp[j] = a[m][i][(j+i) % 4];
      for(j=0; j!=4; j++) a[m][i][j] = tmp[j];
    }
  }
}

/* Replace every byte of the input by the byte at that place in the nonlinear S-box.
 * This operation is performed for each part of the LANE state.
 */
void SubBytes256(lane256_state a) {
  int i, j, m;

  for(m=0; m!=2; m++) { /* for each AES state of the LANE-256 state */
    for(i=0; i!=4; i++) {
      for(j=0; j!=4; j++) {
        a[m][i][j] = S[a[m][i][j]];
      }
    }
  }
}

/* Mix the four bytes of every column in a linear way
 * This operation is performed for each part of the LANE state.
 */
void MixColumns256(lane256_state a) {
  uint8_t b[4][4];
  int i, j, m;

  for(m=0; m!=2; m++) { /* for each AES state of the LANE-256 state */
    for(j = 0; j != 4; j++)
      for(i = 0; i != 4; i++)
        b[i][j] = mul2(a[m][i][j])
                  ^ mul3(a[m][(i + 1) % 4][j])
                  ^ a[m][(i + 2) % 4][j]
                  ^ a[m][(i + 3) % 4][j];
    for(i = 0; i != 4; i++)
      for(j = 0; j != 4; j++) a[m][i][j] = b[i][j];
  }
}

/* XOR all the elements of two LANE states together, put the result in the first one */
void XorLaneState256(lane256_state a, lane256_state b) {
  int i, j, m;
  for(m=0; m!=2; m++) /* for each AES state of the LANE-256 state */
    for (i=0; i!=4; i++)
      for (j=0; j!=4; j++)
        a[m][i][j] ^= b[m][i][j];
}

/* Reorder the columns of a LANE state. */
void SwapColumns256(lane256_state a) {
  int i;
  for (i=0; i!=4; i++) {
    swap_bytes(&a[0][i][2],&a[1][i][0]);
    swap_bytes(&a[0][i][3],&a[1][i][1]);
  }
}

/* Apply permutation P to the input LANE state. */
void PermuteP256(int j,                /* permutation number */
                 lane256_state a,      /* the input LANE state */
                 DataLength counter) { /* the counter value used by AddCounter() */
  int i, r;

  /* rounds-1 ordinary rounds */
  for(i=0; i!=5; i++) {
    r = 5*j+i;
    SubBytes256(a);
    ShiftRows256(a);
    MixColumns256(a);
    AddConstants256(r,a);
    AddCounter256(r,a,counter);
    SwapColumns256(a);
  }
  /* special last round: no AddConstants() nor AddCounter() */
  SubBytes256(a);
  ShiftRows256(a);
  MixColumns256(a);
  SwapColumns256(a);
}

/* Apply permutation Q to the input LANE state. */
void PermuteQ256(int j,                /* permutation number */
                 lane256_state a,      /* the input LANE state */
                 DataLength counter) { /* the counter value used by AddCounter() */
  int i, r;

  /* rounds-1 ordinary rounds */
  for(i=0; i!=2; i++) {
    r = 30+2*j+i;
    SubBytes256(a);
    ShiftRows256(a);
    MixColumns256(a);
    AddConstants256(r,a);
    AddCounter256(r,a,counter);
    SwapColumns256(a);
  }
  /* special last round: no AddConstants() nor AddCounter() */
  SubBytes256(a);
  ShiftRows256(a);
  MixColumns256(a);
  SwapColumns256(a);
}

/* Perform the message expansion. */
void ExpandMessage256(uint8_t *hashval, /* storage for the intermediate hash value */
                      const uint8_t buffer[64], /* the block to be hashed */
                      lane256_state W0,       /* W0 */
                      lane256_state W1,       /* W1 */
                      lane256_state W2,       /* W2 */
                      lane256_state W3,       /* W3 */
                      lane256_state W4,       /* W4 */
                      lane256_state W5) {     /* W5 */

  int i, j;

  /* calculate W_0 (first and second parts) */
  /* W_0a = h_0 ^ m_0 ^ m_1 ^ m_2 ^ m_3 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++)
      W0[0][j][i] = hashval[4*i+j] ^ buffer[4*i+j] ^ buffer[16+4*i+j]
                  ^ buffer[32+4*i+j] ^ buffer[48+4*i+j];
  /* W_0b = h_1 ^ m_0 ^ m_2 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++)
      W0[1][j][i] = hashval[16+4*i+j] ^ buffer[4*i+j] ^ buffer[32+4*i+j];

  /* calculate W_1 (first and second parts) */
  /* W_1a = h_0 ^ h_1  ^ m_0 ^ m_2 ^ m_3 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++)
      W1[0][j][i] = hashval[4*i+j] ^ hashval[16+4*i+j] ^ buffer[4*i+j]
                  ^ buffer[32+4*i+j] ^ buffer[48+4*i+j];
  /* W_1b = h_0 ^ m_1 ^ m_2 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++)
      W1[1][j][i] = hashval[4*i+j] ^ buffer[16+4*i+j] ^ buffer[32+4*i+j];

  /* calculate W_2 (first and second parts) */
  /* W_2a = h_0 ^ h_1  ^ m_0 ^ m_1 ^ m_2 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++)
      W2[0][j][i] = hashval[4*i+j] ^ hashval[16+4*i+j] ^ buffer[4*i+j]
                  ^ buffer[16+4*i+j] ^ buffer[32+4*i+j];
  /* W_2b = h_0 ^ m_0 ^ m_3 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++)
      W2[1][j][i] = hashval[4*i+j] ^ buffer[4*i+j] ^ buffer[48+4*i+j];

  /* calculate W_3 (first and second parts) */
  /* W_3a = h_0 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++)
      W3[0][j][i] = hashval[4*i+j];
  /* W_3b = h_1 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++)
      W3[1][j][i] = hashval[16+4*i+j];

  /* calculate W_4 (first and second parts) */
  /* W_4a = m_0 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++)
      W4[0][j][i] = buffer[4*i+j];
  /* W_4b = m_1 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++)
      W4[1][j][i] = buffer[16+4*i+j];

  /* calculate W_5 (first and second parts) */
  /* W_5a = m_1 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++)
      W5[0][j][i] = buffer[32+4*i+j];
  /* W_5b = m_2 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++)
      W5[1][j][i] = buffer[48+4*i+j];

}
/* Store the hash value. */
void StoreHash256(uint8_t *hashval,     /* storage for the intermediate hash value */
                  lane256_state W0) { /* W0 */
  int i, j, m;
  for(m=0; m!=2; m++) /* for each AES state of the LANE-256 state */
    for (i=0; i!=4; i++)
      for (j=0; j!=4; j++)
        hashval[16*m+4*i+j] = W0[m][j][i];
}

/* Apply the algorithm's compression function to one block of input. */
void Lane256Transform(hashState *state, /* structure that holds hashState information */
                      const uint8_t buffer[64],     /* the block to be hashed */
                      const DataLength counter) { /* counter value */
  lane256_state W0, W1, W2, W3, W4, W5;

  /* expand message */
  ExpandMessage256(state->hash, buffer, W0, W1, W2, W3, W4, W5);

  /* apply permutations */
  /* process the first layer */
  PermuteP256(0,W0,counter);
  PermuteP256(1,W1,counter);
  PermuteP256(2,W2,counter);
  PermuteP256(3,W3,counter);
  PermuteP256(4,W4,counter);
  PermuteP256(5,W5,counter);

  XorLaneState256(W0,W1);
  XorLaneState256(W0,W2);
  XorLaneState256(W3,W4);
  XorLaneState256(W3,W5);

  /* process the second layer */
  PermuteQ256(0,W0,counter);
  PermuteQ256(1,W3,counter);

  XorLaneState256(W0,W3);

  /* store resulting hash value */
  StoreHash256(state->hash, W0);

}
/* ===== */

/* ===== "laneref512.h" */
typedef aes_state lane512_state[4];  /* a LANE-512 state consists of four AES states */

/* Adds a 32-bit constant to each column of the state.
 * This operation is performed for each part of the LANE state.
 */
void AddConstants512(int r, lane512_state a);

/* Adds part of the 64-bit counter to the state.
 * This operation is performed for each part of the LANE state.
 */
void AddCounter512(int r, lane512_state a, DataLength counter);

/* Row 0 remains unchanged, the other three rows are shifted a variable amount.
 * This operation is performed for each part of the LANE state.
 */
void ShiftRows512(lane512_state a);

/* Replace every byte of the input by the byte at that place in the nonlinear S-box.
 * This operation is performed for each part of the LANE state.
 */
void SubBytes512(lane512_state a);

/* Mix the four bytes of every column in a linear way
 * This operation is performed for each part of the LANE state.
 */
void MixColumns512(lane512_state a);

/* XOR all the elements of two LANE states together, put the result in the first one */
void XorLaneState512(lane512_state a, lane512_state b);

/* Reorder the columns of a LANE state. */
void SwapColumns512(lane512_state a);

/* Apply permutation P to the input LANE state. */
void PermuteP512(int j,               /* permutation number */
                 lane512_state a,     /* the input LANE state */
                 DataLength counter); /* the counter value used by AddCounter() */

/* Apply permutation Q to the input LANE state. */
void PermuteQ512(int j,               /* permutation number */
                 lane512_state a,     /* the input LANE state */
                 DataLength counter); /* the counter value used by AddCounter() */

/* Perform the message expansion. */
void ExpandMessage512(uint8_t *hashval,       /* storage for the intermediate hash value */
                   const uint8_t buffer[128], /* the block to be hashed */
                   lane512_state W0,        /* W0 */
                   lane512_state W1,        /* W1 */
                   lane512_state W2,        /* W2 */
                   lane512_state W3,        /* W3 */
                   lane512_state W4,        /* W4 */
                   lane512_state W5);       /* W5 */

/* Store the hash value. */
void StoreHash512(uint8_t* hashval,    /* storage for the intermediate hash value */
                  lane512_state W0); /* W0 */

/* Apply the algorithm's compression function to one block of input. */
void Lane512Transform(hashState *state, /* structure that holds hashState information */
                      const uint8_t buffer[128],   /* the block to be hashed */
                      const DataLength counter); /* counter value */

/* Adds a 32-bit constant to each column of the state.
 * This operation is performed for each part of the LANE state.
 */
void AddConstants512(int r, lane512_state a) {
  int i, j, m;

  for(m=0; m!=4; m++) { /* for each AES state of the LANE-512 state */
    for(i=0; i!=4; i++) {
      for(j=0; j!=4; j++) {
        a[m][j][i] ^= select_byte_32( k[16*r+4*m+i], j);
      }
    }
  }
}

/* Adds part of the 64-bit counter to the state
 * This operation is performed for each part of the LANE state.
 */
void AddCounter512(int r, lane512_state a, DataLength counter) {
  int j;
  for(j=0; j!=4; j++) {
    a[0][j][3] ^= select_byte_64(counter,4*(r%2)+j);
  }
}

/* Row 0 remains unchanged, the other three rows are shifted a variable amount.
 * This operation is performed for each part of the LANE state.
 */
void ShiftRows512(lane512_state a) {

  uint8_t tmp[4];
  int i, j, m;

  for(m=0; m!=4; m++) { /* for each AES state of the LANE-512 state */
    for(i=1; i!=4; i++) {
      for(j=0; j!=4; j++) tmp[j] = a[m][i][(j+i) % 4];
      for(j=0; j!=4; j++) a[m][i][j] = tmp[j];
    }
  }
}

/* Replace every byte of the input by the byte at that place in the nonlinear S-box.
 * This operation is performed for each part of the LANE state.
 */
void SubBytes512(lane512_state a) {
  int i, j, m;

  for(m=0; m!=4; m++) { /* for each AES state of the LANE-512 state */
    for(i=0; i!=4; i++) {
      for(j=0; j!=4; j++) {
        a[m][i][j] = S[a[m][i][j]];
      }
    }
  }
}

/* Mix the four bytes of every column in a linear way
 * This operation is performed for each part of the LANE state.
 */
void MixColumns512(lane512_state a) {
  uint8_t b[4][4];
  int i, j, m;

  for(m=0; m!=4; m++) { /* for each AES state of the LANE-512 state */
    for(j = 0; j != 4; j++)
      for(i = 0; i != 4; i++)
        b[i][j] = mul2(a[m][i][j])
                  ^ mul3(a[m][(i + 1) % 4][j])
                  ^ a[m][(i + 2) % 4][j]
                  ^ a[m][(i + 3) % 4][j];
    for(i = 0; i != 4; i++)
      for(j = 0; j != 4; j++) a[m][i][j] = b[i][j];
  }
}

/* XOR all the elements of two LANE states together, put the result in the first one */
void XorLaneState512(lane512_state a, lane512_state b) {
  int i, j, m;
  for(m=0; m!=4; m++) /* for each AES state of the LANE-512 state */
    for (i=0; i!=4; i++)
      for (j=0; j!=4; j++)
        a[m][i][j] ^= b[m][i][j];
}

/* Reorder the columns of a LANE state. */
void SwapColumns512(lane512_state a) {
  int i;
  for (i=0; i!=4; i++) {
    swap_bytes(&a[0][i][1],&a[1][i][0]);
    swap_bytes(&a[0][i][2],&a[2][i][0]);
    swap_bytes(&a[0][i][3],&a[3][i][0]);
    swap_bytes(&a[1][i][2],&a[2][i][1]);
    swap_bytes(&a[1][i][3],&a[3][i][1]);
    swap_bytes(&a[2][i][3],&a[3][i][2]);
  }
}

/* Apply permutation P to the input LANE state. */
void PermuteP512(int j,                /* permutation number */
                 lane512_state a,      /* the input LANE state */
                 DataLength counter) { /* the counter value used by AddCounter() */
  int i, r;

  /* rounds-1 ordinary rounds */
  for(i=0; i!=7; i++) {
    r = 7*j+i;
    SubBytes512(a);
    ShiftRows512(a);
    MixColumns512(a);
    AddConstants512(r,a);
    AddCounter512(r,a,counter);
    SwapColumns512(a);
  }
  /* special last round: no AddConstants() nor AddCounter() */
  SubBytes512(a);
  ShiftRows512(a);
  MixColumns512(a);
  SwapColumns512(a);
}

/* Apply permutation Q to the input LANE state. */
void PermuteQ512(int j,                /* permutation number */
                 lane512_state a,      /* the input LANE state */
                 DataLength counter) { /* the counter value used by AddCounter() */
  int i, r;

  /* rounds-1 ordinary rounds */
  for(i=0; i!=3; i++) {
    r = 42+3*j+i;
    SubBytes512(a);
    ShiftRows512(a);
    MixColumns512(a);
    AddConstants512(r,a);
    AddCounter512(r,a,counter);
    SwapColumns512(a);
  }
  /* special last round: no AddConstants() nor AddCounter() */
  SubBytes512(a);
  ShiftRows512(a);
  MixColumns512(a);
  SwapColumns512(a);
}

/* Perform the message expansion. */
void ExpandMessage512(uint8_t *hashval, /* storage for the intermediate hash value */
                      const uint8_t buffer[128], /* the block to be hashed */
                      lane512_state W0,        /* W0 */
                      lane512_state W1,        /* W1 */
                      lane512_state W2,        /* W2 */
                      lane512_state W3,        /* W3 */
                      lane512_state W4,        /* W4 */
                      lane512_state W5) {      /* W5 */

  int i, j;

  /* calculate W_0 (four parts) */
  /* W_0a = h_0 ^ m_0 ^ m_1 ^ m_2 ^ m_3 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++) {
      W0[0][j][i] = hashval[4*i+j] ^ buffer[4*i+j] ^ buffer[32+4*i+j]
                  ^ buffer[64+4*i+j] ^ buffer[96+4*i+j];
      W0[1][j][i] = hashval[16+4*i+j] ^ buffer[16+4*i+j] ^ buffer[16+32+4*i+j]
                  ^ buffer[16+64+4*i+j] ^ buffer[16+96+4*i+j];
      }
  /* W_0b = h_1 ^ m_0 ^ m_2 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++) {
      W0[2][j][i] = hashval[32+4*i+j] ^ buffer[4*i+j] ^ buffer[64+4*i+j];
      W0[3][j][i] = hashval[16+32+4*i+j] ^ buffer[16+4*i+j] ^ buffer[16+64+4*i+j];
    }

  /* calculate W_1 (four parts) */
  /* W_1a = h_0 ^ h_1  ^ m_0 ^ m_2 ^ m_3 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++) {
      W1[0][j][i] = hashval[4*i+j] ^ hashval[32+4*i+j] ^ buffer[4*i+j]
                  ^ buffer[64+4*i+j] ^ buffer[96+4*i+j];
      W1[1][j][i] = hashval[16+4*i+j] ^ hashval[16+32+4*i+j] ^ buffer[16+4*i+j]
                  ^ buffer[16+64+4*i+j] ^ buffer[16+96+4*i+j];
    }
  /* W_1b = h_0 ^ m_1 ^ m_2 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++) {
      W1[2][j][i] = hashval[4*i+j] ^ buffer[32+4*i+j] ^ buffer[64+4*i+j];
      W1[3][j][i] = hashval[16+4*i+j] ^ buffer[16+32+4*i+j] ^ buffer[16+64+4*i+j];
    }

  /* calculate W_2 (four parts) */
  /* W_2a = h_0 ^ h_1  ^ m_0 ^ m_1 ^ m_2 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++) {
      W2[0][j][i] = hashval[4*i+j] ^ hashval[32+4*i+j] ^ buffer[4*i+j]
                  ^ buffer[32+4*i+j] ^ buffer[64+4*i+j];
      W2[1][j][i] = hashval[16+4*i+j] ^ hashval[16+32+4*i+j] ^ buffer[16+4*i+j]
                  ^ buffer[16+32+4*i+j] ^ buffer[16+64+4*i+j];
      }
  /* W_2b = h_0 ^ m_0 ^ m_3 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++) {
      W2[2][j][i] = hashval[4*i+j] ^ buffer[4*i+j] ^ buffer[96+4*i+j];
      W2[3][j][i] = hashval[16+4*i+j] ^ buffer[16+4*i+j] ^ buffer[16+96+4*i+j];
    }

  /* calculate W_3 (four parts) */
  /* W_3a = h_0 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++) {
      W3[0][j][i] = hashval[4*i+j];
      W3[1][j][i] = hashval[16+4*i+j];
    }
  /* W_3b = h_1 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++) {
      W3[2][j][i] = hashval[32+4*i+j];
      W3[3][j][i] = hashval[48+4*i+j];
    }

  /* calculate W_4 (four parts) */
  /* W_4a = m_0 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++) {
      W4[0][j][i] = buffer[4*i+j];
      W4[1][j][i] = buffer[16+4*i+j];
    }
  /* W_4b = m_1 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++) {
      W4[2][j][i] = buffer[32+4*i+j];
      W4[3][j][i] = buffer[48+4*i+j];
    }

  /* calculate W_5 (four parts) */
  /* W_5a = m_1 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++) {
      W5[0][j][i] = buffer[64+4*i+j];
      W5[1][j][i] = buffer[80+4*i+j];
    }
  /* W_5b = m_2 */
  for (i=0; i!=4; i++)
    for (j=0; j!=4; j++) {
      W5[2][j][i] = buffer[96+4*i+j];
      W5[3][j][i] = buffer[112+4*i+j];
    }

}
/* Store the hash value. */
void StoreHash512(uint8_t *hashval,     /* storage for the intermediate hash value */
                  lane512_state W0) { /* W0 */
  int i, j, m;
  for(m=0; m!=4; m++) /* for each AES state of the LANE-512 state */
    for (i=0; i!=4; i++)
      for (j=0; j!=4; j++)
        hashval[16*m+4*i+j] = W0[m][j][i];
}

/* Apply the algorithm's compression function to one block of input. */
void Lane512Transform(hashState *state, /* structure that holds hashState information */
                      const uint8_t buffer[128],    /* the block to be hashed */
                      const DataLength counter) { /* counter value */
  lane512_state W0, W1, W2, W3, W4, W5;

  /* expand message */
  ExpandMessage512(state->hash, buffer, W0, W1, W2, W3, W4, W5);

  /* apply permutations */
  /* process the first layer */
  PermuteP512(0,W0,counter);
  PermuteP512(1,W1,counter);
  PermuteP512(2,W2,counter);
  PermuteP512(3,W3,counter);
  PermuteP512(4,W4,counter);
  PermuteP512(5,W5,counter);

  XorLaneState512(W0,W1);
  XorLaneState512(W0,W2);
  XorLaneState512(W3,W4);
  XorLaneState512(W3,W5);

  /* process the second layer */
  PermuteQ512(0,W0,counter);
  PermuteQ512(1,W3,counter);

  XorLaneState512(W0,W3);

  /* store resulting hash value */
  StoreHash512(state->hash, W0);

}
/* ===== */

/* Swap two bytes. */
void swap_bytes(uint8_t *a, uint8_t *b) {
  uint8_t tmp;
  tmp = *a;
  *a = *b;
  *b = tmp;
}

/* Select a byte from a 32-bit word, most significant byte has index 0. */
uint8_t select_byte_32(uint32_t word, int idx) {
  return (word >> (3-idx)*8) & 0xFF;
}

/* Select a byte from a 64-bit word, most significant byte has index 0. */
uint8_t select_byte_64(DataLength word, int idx) {
  return (word >> (7-idx)*8) & 0xFF;
}

/* Multiply an element of GF(2^m) by 2, needed for MixColumns256() and MixColumns512(). */
uint8_t mul2(uint8_t a) {
  return a&0x80 ? (a<<1) ^ 0x1B : a<<1;
}

/* Multiply an element of GF(2^m) by 3, needed for MixColumns256() and MixColumns512(). */
uint8_t mul3(uint8_t a) {
  return mul2(a)^a;
}

/* Initialize the hashState structure. */
HashReturn Init(hashState *state, /* structure that holds hashState information */
                int hashbitlen) { /* length in bits of the hash value */
  int i;

  if (hashbitlen != 224 && hashbitlen != 256 && hashbitlen != 384 && hashbitlen != 512) {
    return BAD_HASHBITLEN; /* unsupported length of the output hash value */
  }
  state->hashbitlen   = hashbitlen;   /* length in bits of the hash value  */
  state->databitcount = 0;            /* number of data bits processed so far */

  /* explicitly calculate IV */
  memset(state->hash, 0, 64);         /* set to zero to calculate the initial IV */
  memset(state->buffer, 0, 128);
  state->buffer[0] = 0x02; /* flag byte 0x02: IV derivation without salt */
  for (i=0; i!=4; i++) {
    state->buffer[i+1] = select_byte_32(state->hashbitlen, i);
  }
  if (hashbitlen <= 256) {
    Lane256Transform(state,state->buffer,0);
  } else {
    Lane512Transform(state,state->buffer,0);
  }

  return SUCCESS;
}

/* Process data using the algorithm's compression function.
 * Precondition: Init() has been called.
 */
HashReturn Update(hashState *state,        /* structure that holds hashState information */
                  const BitSequence *data, /* the data to be hashed */
                  DataLength databitlen) { /* length of the data to be hashed, in bits */

  DataLength bytes = databitlen >> 3; /* length of the data to be hashed, in bytes */
  const int BLOCKSIZE = state->hashbitlen <= 256 ? 64 : 128; /* the block size, in bytes */
  const DataLength buffill = BLOCKSIZE == 64 ?
    (state->databitcount >> 3) & 0x3f : (state->databitcount >> 3) & 0x7f;

  if (state->databitcount & 0x7) {
    return BAD_DATABITLEN; /* last call to Update() was already performed */
  }

  /* If the buffer is not empty, add the data to the buffer and process the buffer */
  if (buffill) {
    /* number of bytes to copy */
    const DataLength n = buffill + bytes > BLOCKSIZE ? BLOCKSIZE-buffill : bytes;
    memcpy(state->buffer + buffill, data, n);
    state->databitcount += n << 3;
    if (buffill + n == BLOCKSIZE) { /* the buffer is full */
      if (state->hashbitlen <= 256) {
        Lane256Transform(state, state->buffer, state->databitcount);
      } else {
        Lane512Transform(state, state->buffer, state->databitcount);
      }
    }
    data += n;
    bytes -= n;
  }

  /* Process as many full blocks as possible directly from the input message */
  while (bytes >= BLOCKSIZE) {
    state->databitcount += BLOCKSIZE << 3;
    if (state->hashbitlen <= 256) {
      Lane256Transform(state, data, state->databitcount);
    } else {
      Lane512Transform(state, data, state->databitcount);
    }
    data += BLOCKSIZE;
    bytes -= BLOCKSIZE;
  }

  /* And finally, save the last, incomplete message block */
  if (bytes || (databitlen & 0x7)) {
    memcpy(state->buffer, data, databitlen & 0x7 ? bytes+1 : bytes); /* also copy partial byte */
    state->databitcount += (bytes << 3) + (databitlen & 0x7);
  }

  return SUCCESS;
}

/* Perform post processing and output filtering and return the final hash value.
 * Precondition: Init() has been called.
 * Final() may only be called once.
 */
HashReturn Final(hashState *state,       /* structure that holds hashState information */
                 BitSequence *hashval) { /* storage for the final hash value */
  int i;
  const int BLOCKSIZE = state->hashbitlen <= 256 ? 64 : 128; /* the block size, in bytes */

  /* If there is unprocessed data in the buffer: zero-pad and compress the last block */
  if (BLOCKSIZE == 64) {
    if (state->databitcount & 0x1ff) {
      /* number of bytes in buffer that are (partially) filled */
      const DataLength n = (((state->databitcount - 1) >> 3) + 1) & 0x3f;
      if (n < BLOCKSIZE)
        memset(state->buffer + n, 0, BLOCKSIZE-n);
      /* zero-pad partial byte */
      state->buffer[(state->databitcount >> 3) & 0x3f]
        &= ~(0xff >> (state->databitcount & 0x7));
      Lane256Transform(state, state->buffer, state->databitcount);
    }
  } else {
    if (state->databitcount & 0x3ff) {
      /* number of bytes in buffer that are (partially) filled */
      const DataLength n = (((state->databitcount - 1) >> 3) + 1) & 0x7f;
      if (n < BLOCKSIZE)
        memset(state->buffer + n, 0, BLOCKSIZE-n);
      /* zero-pad partial byte */
      state->buffer[(state->databitcount >> 3) & 0x7f]
        &= ~(0xff >> (state->databitcount & 0x7));
      Lane512Transform(state, state->buffer, state->databitcount);
    }
  }

  /* output transformation */
  memset(state->buffer, 0, BLOCKSIZE);

  state->buffer[0] = 0x00; /* flag byte 0x00: output transformation without salt */
  /* message length in big endian */
  for (i=0; i!=8; i++) {
    state->buffer[i+1] = select_byte_64(state->databitcount, i);
  }

  /* output transformation */
  if (state->hashbitlen <= 256) {
    Lane256Transform(state, state->buffer, 0);
  } else {
    Lane512Transform(state, state->buffer, 0);
  }

  /* write back result */
  memcpy(hashval, state->hash, state->hashbitlen/8);

  return SUCCESS;
}

/* Hash the supplied data and provide the resulting hash code. */
HashReturn Hash(int hashbitlen,          /* length in bits of desired hash value */
                const BitSequence *data, /* the data to be hashed */
                DataLength databitlen,   /* length of the data to be hashed, in bits */
                BitSequence *hashval) {  /* resulting hash value */
  hashState state;
  HashReturn hashReturn;

  if ((hashReturn = Init(&state, hashbitlen)) != SUCCESS)         return hashReturn;
  if ((hashReturn = Update(&state, data, databitlen)) != SUCCESS) return hashReturn;
  if ((hashReturn = Final(&state, hashval)) != SUCCESS)           return hashReturn;
  return SUCCESS;
}
