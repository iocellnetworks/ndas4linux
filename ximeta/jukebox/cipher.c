#ifdef XPLAT_JUKEBOX
#include <sal/thread.h>
#include <sal/time.h>
#include <sal/sync.h>
#include <sal/mem.h>

#include <xlib/dpc.h>
#include <xlib/xbuf.h>

#include <netdisk/netdiskid.h>
#include <netdisk/ndasdib.h>
#include <netdisk/sdev.h>
#include <netdisk/conn.h>
#include <netdisk/hash.h>
#include "rijndael-api-fst.h"
#include "cipher.h"

#define DEBUG_LEVEL_MSHARE_CIPHER  1

#define    debug_cipher(l, x)    do {\
    if(l <= DEBUG_LEVEL_MSHARE_CIPHER) {     \
        sal_debug_print("UD|%d|%s|",l,__FUNCTION__); \
        sal_debug_println x;    \
    } \
} while(0)    



//////////////////////////////////////////////////////////////////////////
//
//	Cipher instance management.
//


//
//
//
PNCIPHER_INSTANCE
CreateCipher(
	 PNCIPHER_INSTANCE		pCipherInstance,	
	 unsigned char			CipherType,
	 unsigned char			Mode,
	 int				ExtraKeyLength,
	 unsigned char *		ExtraKey
){
	int			instLength;
	PNCIPHER_INSTANCE	tmpCipher;
	int			ret;
	
	
	switch(CipherType)
	{
	case NDAS_CIPHER_SIMPLE: {
		PCIPHER_HASH	cipher;
	
		if(!ExtraKey) {
			debug_cipher(2,("Hash key is NULL.\n"));
			return NULL;
		}
		if(ExtraKeyLength != HASH_KEY_LENGTH) {
			debug_cipher(2,("Hash key length invalid. ExtraKeyLength=%d\n", ExtraKeyLength));
			return NULL;
		}

		instLength = sizeof(NCIPHER_INSTANCE) - sizeof(unsigned char) + sizeof(CIPHER_HASH);
		tmpCipher = pCipherInstance;
		sal_memset(tmpCipher,0, instLength);
		tmpCipher->CipherType = NDAS_CIPHER_SIMPLE;
		tmpCipher->InstanceSpecificLength = sizeof(CIPHER_HASH);

		//
		//	Set Hash key.
		//
		cipher = (PCIPHER_HASH)tmpCipher->InstanceSpecific;
		sal_memcpy(cipher->HashKey, ExtraKey, HASH_KEY_LENGTH);

	}
	break;
	case NDAS_CIPHER_AES:
		instLength = sizeof(NCIPHER_INSTANCE) - sizeof(unsigned char) + sizeof(NCIPHER_INSTANCE);
		tmpCipher = pCipherInstance;
		sal_memset(tmpCipher,0, instLength);
		tmpCipher->CipherType = NDAS_CIPHER_AES;
		tmpCipher->InstanceSpecificLength = sizeof(cipherInstance);

		ret = cipherInit((cipherInstance *)tmpCipher->InstanceSpecific, Mode, NULL);
		if(ret != TRUE) {
			//kfree(tmpCipher);
			debug_cipher(2,("Could not initialize the Cipher. ERRORCODE:%d\n", ret));
			return NULL;
		}
		

	break;
	default:
		return NULL;
	}
	return tmpCipher;
}

int
CloseCipher(
	 PNCIPHER_INSTANCE	Cipher
){
	if(!Cipher) {
		debug_cipher(2,("Cipher parameter is NULL!\n"));
		return -1;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
//
//	Key instance management.
//

//
//
//
PNCIPHER_KEY
CreateCipherKey(
	PNCIPHER_KEY		pCipherKey,
	PNCIPHER_INSTANCE	Cipher,
	int			KeyBinaryLength,
	unsigned char *		KeyBinary
){
	int					keyLength;
	PNCIPHER_KEY		tmpKey;
	int					ret;
	if(KeyBinaryLength > NCIPHER_MAX_KEYLENGTH) {
		debug_cipher(2,("Key length too large. KeyBinaryLen=%d\n", KeyBinaryLength));
		return NULL;
	}

	switch(Cipher->CipherType)
	{
	case NDAS_CIPHER_SIMPLE: {
		PCIPHER_HASH		hash;
		PCIPHER_HASH_KEY	hashKey;

		if(KeyBinaryLength != sizeof(unsigned int)) {
			debug_cipher(2,("Key length invalid. KeyBinaryLen=%d\n", KeyBinaryLength));
			return NULL;
		}

		keyLength = sizeof(NCIPHER_KEY) - sizeof(unsigned char) + sizeof(CIPHER_HASH_KEY);
		tmpKey = pCipherKey;
		sal_memset(tmpKey,0, keyLength);
		tmpKey->CipherType = NDAS_CIPHER_SIMPLE;
		tmpKey->CipherSpecificKeyLength = sizeof(CIPHER_HASH_KEY);
		tmpKey->KeyBinaryLength = KeyBinaryLength * 8;
		sal_memcpy(tmpKey->KeyBinary, KeyBinary, KeyBinaryLength);

		//
		//	Calculate intermediate values.
		//
		hashKey = (PCIPHER_HASH_KEY)tmpKey->CipherSpecificKey;
		hash = (PCIPHER_HASH)Cipher->InstanceSpecific;

		hashKey->CntEcr_IR[0] = hash->HashKey[1] ^ hash->HashKey[7] ^ KeyBinary[3];
		hashKey->CntEcr_IR[1] = hash->HashKey[0] ^ hash->HashKey[3] ^ KeyBinary[0];
		hashKey->CntEcr_IR[2] = hash->HashKey[2] ^ hash->HashKey[6] ^ KeyBinary[2];
		hashKey->CntEcr_IR[3] = hash->HashKey[4] ^ hash->HashKey[5] ^ KeyBinary[1];

		hashKey->CntDcr_IR[0] = ~(hash->HashKey[0] ^ hash->HashKey[3] ^ KeyBinary[0]);
		hashKey->CntDcr_IR[1] = hash->HashKey[2] ^ hash->HashKey[6] ^ KeyBinary[2];
		hashKey->CntDcr_IR[2] = hash->HashKey[4] ^ hash->HashKey[5] ^ ~(KeyBinary[1]);
		hashKey->CntDcr_IR[3] = hash->HashKey[1] ^ hash->HashKey[7] ^ KeyBinary[3];

	}
	break;
	case NDAS_CIPHER_AES:
		keyLength = sizeof(NCIPHER_KEY) - sizeof(unsigned char) + sizeof(keyInstance);
		/*
		tmpKey = (PNCIPHER_KEY)kmalloc(keyLength, GFP_KERNEL);
		if(!tmpKey) {
			debug_cipher("Could not allocate Cipher key.\n");
			return NULL;
		}
		*/
		tmpKey = pCipherKey;
		sal_memset(tmpKey,0, keyLength);
		tmpKey->CipherType = NDAS_CIPHER_AES;
		tmpKey->CipherSpecificKeyLength = sizeof(keyInstance);
		tmpKey->KeyBinaryLength = KeyBinaryLength * 8;
		sal_memcpy(tmpKey->KeyBinary, KeyBinary, KeyBinaryLength);
		
		ret = makeKeyEncDec((keyInstance *)tmpKey->CipherSpecificKey, tmpKey->KeyBinaryLength, (char *)KeyBinary);
		
		if(ret != TRUE) {
			//kfree(tmpKey);
			debug_cipher(2,("Could not initialize the Cipher Key.\n"));
			return NULL;
		}

		{
			keyInstance * pKey = (keyInstance *)tmpKey->CipherSpecificKey;
			int i = 0;
			for(i = 0; i < (int)(pKey->keyLen / 8); i++)
			{
				if(!(i%8)) debug_cipher(2,("\n"));
				debug_cipher(2,("[%d]0x%02x", i, pKey->keyMaterial[i]));
			}
			debug_cipher(2,("\n"));

			debug_cipher(2,("rke"));
			for(i = 0; i < 60; i++)
			{
				if(!(i%8)) debug_cipher(2,("\n"));
				debug_cipher(2,("[%d]0x%d", i, pKey->rke[i]));
			}
			debug_cipher(2,("\n"));

			debug_cipher(2,("rkd"));
			for(i = 0; i < 60; i++)
			{
				if(!(i%8)) debug_cipher(2,("\n"));
				debug_cipher(2,("[%d]0x%d", i, pKey->rkd[i]));
			}
			debug_cipher(2,("\n"));

			debug_cipher(2,("ek"));
			for(i = 0; i < 60; i++)
			{
				if(!(i%8)) debug_cipher(2,("\n"));
				debug_cipher(2,("[%d]0x%d", i, pKey->ek[i]));
			}
			debug_cipher(2,("\n"));

		}	

	break;
	default:
		return NULL;
	}
	return tmpKey;
}

int
CloseCipherKey(
	PNCIPHER_KEY key
){
	if(!key) {
		debug_cipher(2,("Cipher parameter is NULL!\n"));
		return -1;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
//
//	Encrypt/decrypt
//

int
EncryptBlock(
	PNCIPHER_INSTANCE	Cipher,
	PNCIPHER_KEY		Key,
	unsigned int		BufferLength,
	unsigned char *		InBuffer,
	unsigned char *		OutBuffer
){
	int ret;

	if(!InBuffer || !OutBuffer) {
		debug_cipher(2,("Buffer parameter is NULL!\n"));
		return -1;
	}

	switch(Cipher->CipherType) {
	case	NDAS_CIPHER_SIMPLE: {
		PCIPHER_HASH_KEY	key = (PCIPHER_HASH_KEY)Key->CipherSpecificKey;


		if(InBuffer != OutBuffer) {
			Encrypt32SPAndCopy(
					OutBuffer,
					InBuffer,
					BufferLength,
					(unsigned char *)&key->CntEcr_IR[0]
				);
		} else {
			Encrypt32SP(
					InBuffer,
					BufferLength,
					(unsigned char *)&key->CntEcr_IR[0]
				);
		}
		break;
	}
	case	NDAS_CIPHER_AES: {
		keyInstance *aesKey = (keyInstance *)Key->CipherSpecificKey;
		aesKey->direction = DIR_ENCRYPT;
		ret = blockEncrypt(
				(cipherInstance *)Cipher->InstanceSpecific,
				aesKey,
				InBuffer,				// Input buffer
				BufferLength<<3,		// bits
				OutBuffer				// output buffer
				);
		if(ret < 0) {
			debug_cipher(2,("blockEncrypt() failed. Ret=%d.\n", ret));
			return -1;
		}
		break;
	}
	default:
		return -1;
	}
	return 0;
}

int
EncryptPad(
	PNCIPHER_INSTANCE	Cipher,
	PNCIPHER_KEY		Key,
	unsigned char *		InBuffer,
	int			InputOctets,
	unsigned char *		OutBuffer
){

	return -1;
}

int
DecryptBlock(
	PNCIPHER_INSTANCE	Cipher,
	PNCIPHER_KEY		Key,
	int			BufferLength,
	unsigned char *		InBuffer,
	unsigned char *		OutBuffer
){
	int ret;

	if(!InBuffer || !OutBuffer) {
		debug_cipher(2,("Buffer parameter is NULL!\n"));
		return -1;
	}

	switch(Cipher->CipherType) {
	case	NDAS_CIPHER_SIMPLE: {
		PCIPHER_HASH_KEY key = (PCIPHER_HASH_KEY)Key->CipherSpecificKey;
		debug_cipher(2,("blockDecrypt() NDAS_CIPHER_SIMPLE called\n"));


		if(InBuffer != OutBuffer) {
			debug_cipher(2,("Does not support encryption-copy!\n"));
			return -1;
		} else {
			Decrypt32SP(
					InBuffer,
					BufferLength,
					(unsigned char *)&key->CntDcr_IR[0]
				);
		}
		break;
	}
	case	NDAS_CIPHER_AES: {
		keyInstance *aesKey = (keyInstance *)Key->CipherSpecificKey;
		debug_cipher(2,("blockDecrypt() NDAS_CIPHER_AES called\n"));

		aesKey->direction = DIR_DECRYPT;
		ret = blockDecrypt(
					(cipherInstance *)Cipher->InstanceSpecific,
					aesKey,
					InBuffer,			// Input buffer
					BufferLength<<3,		// bits
					OutBuffer			// output buffer
				);
		if(ret < 0) {
			debug_cipher(2,("blockDecrypt() failed. Ret=%d.\n", ret));
			return -1;
		}
		break;
	}
	default:
		debug_cipher(2,("blockDecrypt() Undefined called\n"));
		return -1;
	}
	return 0;
}

int
DecryptPad(
	PNCIPHER_INSTANCE	Cipher,
	PNCIPHER_KEY		Key,
	unsigned char *		InBuffer,
	int			InputOctets,
	unsigned char *		OutBuffer
){

	return -1;
}
#endif //#ifdef XPLAT_JUKEBOX
