// Copyright (c) 2020 Cesanta Software Limited
// All rights reserved
//
// Example Websocket server. See https://mongoose.ws/tutorials/websocket-server/

#include "mongoose.h"
#include <winscard.h>
#pragma comment(lib, "winscard.lib")

static const char *s_listen_on = "ws://localhost:8000";
static const char *s_web_root = ".";




bool GetCardNumber(char *pszReturn) {// Parameter must have space for 6 chars
  SCARDCONTEXT hSC;
  LONG lReturn;
  // Establish the context.
  lReturn = SCardEstablishContext(SCARD_SCOPE_USER, NULL, NULL, &hSC);
  if (SCARD_S_SUCCESS != lReturn)
    printf("Failed SCardEstablishContext\n");
  else {

    LPTSTR pmszReaders = NULL;
    LPTSTR pReader;
    LPTSTR pszReader = NULL;
    LONG lReturn, lReturn2;
    DWORD cch = SCARD_AUTOALLOCATE;

    // Retrieve the list the readers.
    // hSC was set by a previous call to SCardEstablishContext.
    lReturn = SCardListReaders(hSC, NULL, (LPTSTR) &pmszReaders, &cch);
    switch (lReturn) {
      case SCARD_E_NO_READERS_AVAILABLE:
        printf("Reader is not in groups.\n");
        // Take appropriate action.
        // ...
        break;

      case SCARD_S_SUCCESS:
        // Do something with the multi string of readers.
        // Output the values.
        // A double-null terminates the list of values.
        pReader = pmszReaders;
        while ('\0' != *pReader) {
          // Display the value.
          printf("Reader: %S\n", pReader);
          // Advance to the next value.
          pReader = pReader + wcslen((wchar_t *) pReader) + 1;
        }
        /*LPTSTR*/ pszReader = pmszReaders;  // take first reader

        SCARDHANDLE hCardHandle;
        LONG lReturn;
        DWORD dwAP;

        lReturn = SCardConnect(
            hSC, pszReader,
            SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
            &hCardHandle, &dwAP);
        if (SCARD_S_SUCCESS != lReturn) {
          printf("Failed SCardConnect\n");
          return false;//exit(1);  // Or other appropriate action.
        }

        // Use the connection.
        // Display the active protocol.
        switch (dwAP) {
          case SCARD_PROTOCOL_T0:
            printf("Active protocol T0\n");
            break;

          case SCARD_PROTOCOL_T1:
            printf("Active protocol T1\n");
            break;

          case SCARD_PROTOCOL_UNDEFINED:
          default:
            printf("Active protocol unnegotiated or unknown\n");
            break;
        }

        //  Transmit the request.
        //  lReturn is of type LONG.
        //  hCardHandle was set by a previous call to SCardConnect.
        //  pbSend points to the buffer of bytes to send.
        //  dwSend is the DWORD value for the number of bytes to send.
        //  pbRecv points to the buffer for returned bytes.
        //  dwRecv is the DWORD value for the number of returned bytes.
        // struct {
        //BYTE bCla,  // the instruction class
        //     bIns,   // the instruction code
        //     bP1,    // parameter to the instruction
        //     bP2,    // parameter to the instruction
        //     bP3;    // size of I/O transfer
        //} CmdBytes;
        //ff 82 01 00 06 FF FF FF FF FF FF =load key
        //Load Authentication Keys FFh 82h "Key Structure" "Key Number" 06h Key(6 bytes)
        {
          byte pbRecv[20];
          memset(pbRecv, 0, sizeof(pbRecv));
          DWORD dwRecv = sizeof(pbRecv);
          byte pbSend[] = {0xff, 0x82, 0x01, 00, 06,
                          0xff, 0xff, 0xff, 0xff, 0xff, 0xff};// load key

          lReturn = SCardTransmit(hCardHandle, SCARD_PCI_T1, pbSend,
                                  sizeof(pbSend), NULL, pbRecv, &dwRecv);
          if (SCARD_S_SUCCESS != lReturn) {
            printf("Failed SCardTransmit Load Key\n");
            return false;//exit(1);  // or other appropriate error action
          } else {
            printf("Load key=");
            for (int i = 0; i < dwRecv; i++) {
              printf("%02x ", pbRecv[i]);
            }
            printf("\n");
          }
        }

        //FF8600 00 050100 28 6000 authenticate block 0x28
        {
          byte pbRecv[20];
          memset(pbRecv, 0, sizeof(pbRecv));
          DWORD dwRecv = sizeof(pbRecv);
          byte pbSend[] = {0xff, 0x86, 0x00, 00,   05,
                          0x01, 0x00, /*0x28*/0, 0x60, 0x00};  // load key (5=length of bytes following)

          lReturn = SCardTransmit(hCardHandle, SCARD_PCI_T1, pbSend,
                                  sizeof(pbSend), NULL, pbRecv, &dwRecv);
          if (SCARD_S_SUCCESS != lReturn) {
            printf("Failed SCardTransmit Authenticate Block 0x28\n");
            return false;//exit(1);  // or other appropriate error action
          } else {
            printf("Authenticate Block 0x0=");
            for (int i = 0; i < dwRecv; i++) {
              printf("%02x ", pbRecv[i]);
            }
            printf("\n");
          }
        }

        //ffb0 00 28 10 read block 0x28 length 0x10
        {
          byte pbRecv[20];
          memset(pbRecv, 0, sizeof(pbRecv));
          /* for (int i = 0; i < 20; i++) {
            pbRecv[i] = i + 1;
          }*/
          DWORD dwRecv = sizeof(pbRecv);
          byte pbSend[] = {0xff, 0xb0, 0x00, /*0x28*/0, 0x10};  // bytes following)

          lReturn = SCardTransmit(hCardHandle, SCARD_PCI_T1, pbSend,
                                  sizeof(pbSend), NULL, pbRecv, &dwRecv);
          if (SCARD_S_SUCCESS != lReturn) {
            printf("Failed SCardTransmit Read Block 0x28\n");
            return false;//exit(1);  // or other appropriate error action
          } else {
            printf("Read Block 0x0=");
            for (int i = 0; i < dwRecv; i++) {
              printf("%02x ", pbRecv[i]);
            }
            printf("\n");
          }
        }

        
        // FF8600 00 050100 28 6000 authenticate block 0x28
        {
          byte pbRecv[20];
          memset(pbRecv, 0, sizeof(pbRecv));
          DWORD dwRecv = sizeof(pbRecv);
          byte pbSend[] = {0xff, 0x86, 0x00, 00,  05, 0x01,
                           0x00, 0x28, 0x60, 0x00};  // load key (5=length of
                                                     // bytes following)

          lReturn = SCardTransmit(hCardHandle, SCARD_PCI_T1, pbSend,
                                  sizeof(pbSend), NULL, pbRecv, &dwRecv);
          if (SCARD_S_SUCCESS != lReturn) {
            printf("Failed SCardTransmit Authenticate Block 0x28\n");
            return false;//exit(1);  // or other appropriate error action
          } else {
            printf("Authenticate Block 0x28=");
            for (int i = 0; i < dwRecv; i++) {
              printf("%02x ", pbRecv[i]);
            }
            printf("\n");
          }
        }

        // ffb0 00 28 10 read block 0x28 length 0x10
        {
          byte pbRecv[20];
          memset(pbRecv, 0, sizeof(pbRecv));
          DWORD dwRecv = sizeof(pbRecv);
          byte pbSend[] = {0xff, 0xb0, 0x00, 0x28, 0x10};  // bytes following)

          lReturn = SCardTransmit(hCardHandle, SCARD_PCI_T1, pbSend,
                                  sizeof(pbSend), NULL, pbRecv, &dwRecv);
          if (SCARD_S_SUCCESS != lReturn) {
            printf("Failed SCardTransmit Read Block 0x28\n");
            return false;//exit(1);  // or other appropriate error action
          } else {
            printf("Read Block 0x28=");
            for (int i = 0; i < dwRecv; i++) {
              printf("%02x ", pbRecv[i]);
            }
            printf("\n");
            for (int i = 1; i < 7; i++) {
              pszReturn[i - 1] = pbRecv[i];
              printf("%c", pbRecv[i]);
            }
            printf("\n");
          }
        }

        // Remember to disconnect (by calling SCardDisconnect).
        SCardDisconnect(hSC, SCARD_LEAVE_CARD);

        // Free the memory.
        lReturn2 = SCardFreeMemory(hSC, pmszReaders);
        if (SCARD_S_SUCCESS != lReturn2) printf("Failed SCardFreeMemory\n");
        break;

      default:
        printf("Failed SCardListReaders\n");
        // Take appropriate action.
        // ...
        return false;
        break;
    }


    // Use the context as needed. When done,
    // free the context by calling SCardReleaseContext.
    SCardReleaseContext(hSC);
    return true;
  }

}



// This RESTful server implements the following endpoints:
//   /websocket - upgrade to Websocket, and implement websocket echo server
//   /rest - respond with JSON string {"result": 123}
//   any other URI serves static files from s_web_root
static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  if (ev == MG_EV_OPEN) {
    // c->is_hexdumping = 1;
  } else if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    if (mg_http_match_uri(hm, "/cardnr")) {
      // Upgrade to websocket. From now on, a connection is a full-duplex
      // Websocket connection, which will receive MG_EV_WS_MSG events.
      mg_ws_upgrade(c, hm, NULL);
    } else if (mg_http_match_uri(hm, "/rest")) {
      // Serve REST response
      mg_http_reply(c, 200, "", "{\"result\": %d}\n", 123);
    } else {
      // Serve static files
      struct mg_http_serve_opts opts = {.root_dir = s_web_root};
      // mg_http_serve_dir(c, ev_data, &opts);
      mg_http_serve_dir(c, hm, &opts);
    }
  } else if (ev == MG_EV_WS_MSG) {
    // Got websocket frame. Received data is wm->data. Echo it back!
    struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
    // mg_ws_send(c, wm->data.ptr, wm->data.len, WEBSOCKET_OP_TEXT);
    //char cNumber[] = "203508";
    char cNumber[7];
    memset(cNumber, 0, sizeof(cNumber));
    if (GetCardNumber(cNumber) == true) {
      mg_ws_send(c, cNumber, strlen(cNumber), WEBSOCKET_OP_TEXT);
    } else {
      char szText[] = "failed";
      mg_ws_send(c, szText, strlen(szText), WEBSOCKET_OP_TEXT);
    }
  }
  (void) fn_data;
}

int main(void) {

  //GetCardNumber();
  //return 0;
  struct mg_mgr mgr;  // Event manager
  mg_mgr_init(&mgr);  // Initialise event manager
  printf("Starting WS listener on %s/websocket\n", s_listen_on);
  mg_http_listen(&mgr, s_listen_on, fn, NULL);  // Create HTTP listener
  for (;;) mg_mgr_poll(&mgr, 1000);             // Infinite event loop
  mg_mgr_free(&mgr);
  return 0;
}