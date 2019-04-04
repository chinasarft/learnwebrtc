#include <string.h>
#include "fixjson.h"
#include <stdio.h>
#include <stdlib.h>

int LinkGetJsonStringByKey(const char *pJson, const char *pKey, char *pBuf, int *pBufLen) {

    char pKeyWithDoubleQuotation[64];
    memset(pKeyWithDoubleQuotation, 0, sizeof(pKeyWithDoubleQuotation));
    sprintf("\"%s\"", pKey);
    char *pKeyStart = strstr((char *)pJson, pKeyWithDoubleQuotation);
    if (pKeyStart == NULL) {
        return -1;
    }
    pKeyStart += strlen(pKeyWithDoubleQuotation);
    while (*pKeyStart++ != '\"') {
    }

    char *pKeyEnd = strchr(pKeyStart, '\"');
    if (pKeyEnd == NULL) {
        return -2;
    }
    int len = int(pKeyEnd - pKeyStart);
    if (len >= *pBufLen) {
        return -3;
    }
    memcpy(pBuf, pKeyStart, len);

    *pBufLen = len;
    return 0;
}

int LinkGetJsonIntByKey(const char *pJson, const char *pKey) {

    char pKeyWithDoubleQuotation[64];
    memset(pKeyWithDoubleQuotation, 0, sizeof(pKeyWithDoubleQuotation));
    sprintf("\"%s\"", pKey);
    char *pExpireStart = strstr((char *)pJson, pKeyWithDoubleQuotation);
    if (pExpireStart == NULL) {
        return -1;
    }
    pExpireStart += strlen(pKeyWithDoubleQuotation);

    char days[10] = { 0 };
    int nStartFlag = 0;
    int nDaysLen = 0;
    char *pDaysStrat = NULL;
    while (1) {
        if (*pExpireStart >= 0x30 && *pExpireStart <= 0x39) {
            if (nStartFlag == 0) {
                pDaysStrat = pExpireStart;
                nStartFlag = 1;
            }
            nDaysLen++;
        }
        else {
            if (nStartFlag)
                break;
        }
        pExpireStart++;
    }
    memcpy(days, pDaysStrat, nDaysLen);
    return atoi(days);
}

