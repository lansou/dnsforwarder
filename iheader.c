#include <string.h>
#include "iheader.h"
#include "dnsparser.h"
#include "utils.h"

void IHeader_Reset(IHeader *h)
{
    h->_Pad = 0;
    h->Agent[0] = '\0';
    h->BackAddress.family = AF_UNSPEC;
    h->Domain[0] = '\0';
    h->EDNSEnabled = FALSE;
}

int IHeader_Fill(IHeader *h,
                 BOOL ReturnHeader,
                 char *DnsEntity,
                 int EntityLength,
                 struct sockaddr *BackAddress,
                 SOCKET SendBackSocket,
                 sa_family_t Family,
                 const char *Agent
                 )
{
    DnsSimpleParser p;
    DnsSimpleParserIterator i;

    h->_Pad = 0;

    if( DnsSimpleParser_Init(&p, DnsEntity, EntityLength, FALSE) != 0 )
    {
        return -31;
    }

    if( DnsSimpleParserIterator_Init(&i, &p) != 0 )
    {
        return -36;
    }

    while( i.Next(&i) != NULL )
    {
        if( i.Klass != DNS_CLASS_IN )
        {
            return -42;
        }

        switch( i.Purpose )
        {
        case DNS_RECORD_PURPOSE_QUESTION:
            if( i.GetName(&i, h->Domain, sizeof(h->Domain)) < 0 )
            {
                return -46;
            }

            StrToLower(h->Domain);
            h->HashValue = ELFHash(h->Domain, 0);
            h->Type = (DNSRecordType)DNSGetRecordType(DNSJumpHeader(DnsEntity));
            break;

        case DNS_RECORD_PURPOSE_ADDITIONAL:
            if( i.Type == DNS_TYPE_OPT )
            {
                h->EDNSEnabled = TRUE;
            }
            break;

        default:
            break;
        }
    }

    h->ReturnHeader = ReturnHeader;

    if( BackAddress != NULL )
    {
        memcpy(&(h->BackAddress.Addr), BackAddress, GetAddressLength(Family));
        h->BackAddress.family = Family;
    }

    h->SendBackSocket = SendBackSocket;

    if( Agent != NULL )
    {
        strncpy(h->Agent, Agent, sizeof(h->Agent));
        h->Agent[sizeof(h->Agent) - 1] = '\0';
    } else {
        h->Agent[0] = '\0';
    }

    return 0;
}

int IHeader_SendBack(IHeader *h /* Entity followed */, int FullLength)
{
    if( h->ReturnHeader )
    {
        return sendto(h->SendBackSocket,
                      (const char *)h,
                      FullLength,
                      0,
                      (const struct sockaddr *)&(h->BackAddress.Addr),
                      GetAddressLength(h->BackAddress.family)
                      );
    } else {
        return sendto(h->SendBackSocket,
                      (const char *)(h + 1),
                      FullLength - sizeof(IHeader),
                      0,
                      (const struct sockaddr *)&(h->BackAddress.Addr),
                      GetAddressLength(h->BackAddress.family)
                      );
    }
}
