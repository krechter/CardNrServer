// Copyright (c) 2020 Cesanta Software Limited
// All rights reserved
//
// Example Websocket server. See https://mongoose.ws/tutorials/websocket-server/

#include "mongoose.h"
#include <winscard.h>
#pragma comment(lib, "winscard.lib")

static const char *s_listen_on = "ws://localhost:8000";
static const char *s_web_root = ".";

bool GetFirstReader(SCARDCONTEXT hSC, char* pszReturn, size_t size) {
  LPTSTR pmszReaders = NULL;
  LPTSTR pReader;
  LONG lReturn;
  DWORD cch = SCARD_AUTOALLOCATE;

  // Retrieve the list the readers.
  // hSC was set by a previous call to SCardEstablishContext.
  lReturn = SCardListReaders(hSC, NULL, (LPTSTR)&pmszReaders, &cch);
	//lReturn = SCardListReadersA(hSC, NULL, (LPSTR) & pmszReaders, &cch);
  switch (lReturn) {
  case SCARD_E_NO_READERS_AVAILABLE:
    printf("No Reader available\n");
    strncpy(pszReturn, "No Reader available", size);
    break;

  case SCARD_S_SUCCESS:
    // A double-null terminates the list of values.
    pReader = pmszReaders;
    while ('\0' != *pReader) {
      printf("Reader: %S\n", pReader);
      pReader = pReader + wcslen((wchar_t*)pReader) + 1;
    }
    
		wcsncpy((LPTSTR)pszReturn, pmszReaders, size/2); // take first reader, size/2 wcsncpy copies 2 bytes per character
		
		SCardFreeMemory(hSC, pmszReaders);
    return true;
    break;

  default:
    printf("SCardListReaders failed\n");
    strncpy(pszReturn, "SCardListReaders failed", size);
    break;
  }
  return false;
}

bool Connect(SCARDCONTEXT hSC, char* pszReader, size_t lenReader, LPSCARDHANDLE pCardHandle, char* pszReturn, size_t size) {
	DWORD dwAP;

	LONG lReturn = SCardConnect(hSC, (LPCWSTR)pszReader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, pCardHandle, &dwAP);
	if (SCARD_S_SUCCESS != lReturn) {
		printf("SCardConnect failed \n");
		
		if (lReturn == SCARD_W_REMOVED_CARD) {
			strncpy(pszReturn, "SCardConnect SCARD_W_REMOVED_CARD", size);
		}
		else {
			strncpy(pszReturn, "SCardConnect failed", size);
		}
		return false;
	}
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
	return true;
}

bool LoadKey(SCARDHANDLE hCardHandle, char *pszReturn, size_t size) {
	byte pbRecv[20];
	memset(pbRecv, 0, sizeof(pbRecv));
	DWORD dwRecv = sizeof(pbRecv);
	byte pbSend[] = { 0xff, 0x82, 0x01, 00, 06,
									0xff, 0xff, 0xff, 0xff, 0xff, 0xff };// load key

	LONG lReturn = SCardTransmit(hCardHandle, SCARD_PCI_T1, pbSend, sizeof(pbSend), NULL, pbRecv, &dwRecv);
	if (SCARD_S_SUCCESS != lReturn) {
		printf("SCardTransmit Load Key failed\n");
		strncpy(pszReturn, "SCardTransmit Load Key failed", size);
		return false;
	}	else {
		printf("Load key=");
		for (int i = 0; i < (int)dwRecv; i++) {
			printf("%02x ", pbRecv[i]);
		}
		printf("\n");
	}
	return true;
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
bool AuthenticateBlock(SCARDHANDLE hCardHandle, char Block, char* pszReturn, size_t size) {
	// FF8600 00 050100 28 6000 authenticate block 0x28
	byte pbRecv[20];
	memset(pbRecv, 0, sizeof(pbRecv));
	DWORD dwRecv = sizeof(pbRecv);
	byte pbSend[] = { 0xff, 0x86, 0x00, 00,  05, 0x01, 0x00, (byte)Block, 0x60, 0x00 };  // load key (5=length of bytes following)

	LONG lReturn = SCardTransmit(hCardHandle, SCARD_PCI_T1, pbSend, sizeof(pbSend), NULL, pbRecv, &dwRecv);
	if (SCARD_S_SUCCESS != lReturn) {
		printf("SCardTransmit Authenticate Block %02x failed\n", Block);
		sprintf(pszReturn, "SCardTransmit Authenticate Block %02x failed\n", Block);
		return false;
	}
	return true;
}

bool ReadBlock(SCARDHANDLE hCardHandle, char Block, char* pszReturn, size_t size) {
	// ffb0 00 28 10 read block 0x28 length 0x10
	byte pbRecv[20];
	memset(pbRecv, 0, sizeof(pbRecv));
	DWORD dwRecv = sizeof(pbRecv);
	byte pbSend[] = { 0xff, 0xb0, 0x00, 0x28, 0x10 };  // bytes following)

	LONG lReturn = SCardTransmit(hCardHandle, SCARD_PCI_T1, pbSend, sizeof(pbSend), NULL, pbRecv, &dwRecv);
	if (SCARD_S_SUCCESS != lReturn) {
		printf("SCardTransmit Read Block %02x failed\n", Block);
		sprintf(pszReturn, "SCardTransmit Read Block %02x failed\n", Block);
		return false;
	}

	printf("Read Block %02x=", Block);
	for (int i = 0; i < (int)dwRecv; i++) {
		printf("%02x ", pbRecv[i]);
	}
	printf("\n");
	for (int i = 1; i < 7; i++) {
		pszReturn[i - 1] = pbRecv[i];
	}
	pszReturn[6] = 0;
	printf("%s\n", pszReturn);
	return true;
}

bool GetCardNumber(char *pszReturn, size_t size) {// String must have space for 6 chars
  bool bError = false;
  SCARDCONTEXT hSC;
  LONG lReturn;
  // Establish the context.
  lReturn = SCardEstablishContext(SCARD_SCOPE_USER, NULL, NULL, &hSC);
  if (SCARD_S_SUCCESS != lReturn) {
    printf("Failed SCardEstablishContext\n");
    strncpy(pszReturn, "SCardEstablishContext failed", size);
		return false;
  }
  else {
		const size_t lenReader = 255;
		char pszReader[lenReader];
		if (GetFirstReader(hSC, pszReader, lenReader-1)) {
			SCARDHANDLE hCardHandle;

			if (Connect(hSC, pszReader, lenReader-1, &hCardHandle, pszReturn, size)) {
				if (AuthenticateBlock(hCardHandle, 0x28, pszReturn, size)) {
					if (ReadBlock(hCardHandle, 0x28, pszReturn, size)) {
					
					}	else {
						bError = true;
					}
				}	else {
					bError = true;
				}
				SCardDisconnect(hSC, SCARD_LEAVE_CARD);
			}	else {
				bError = true;
			}
		}  
		SCardReleaseContext(hSC);
  }
  return bError == false;
}



// This RESTful server implements the following endpoints:
//   /cardnr - upgrade to Websocket, and implement websocket echo server
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
    char cNumber[255];
		size_t size = sizeof(cNumber) - 1;
    //memset(cNumber, 0, sizeof(cNumber));
    if (GetCardNumber(cNumber, size) == true) {
      mg_ws_send(c, cNumber, strlen(cNumber), WEBSOCKET_OP_TEXT);
    } else {
      //char szText[] = "failed";
      printf("GetCardNumber failed\n");
      mg_ws_send(c, cNumber, strlen(cNumber), WEBSOCKET_OP_TEXT);
    }
  }
  (void) fn_data;
}

int main(void) {

  //GetCardNumber();
  //return 0;
  struct mg_mgr mgr;  // Event manager
  mg_mgr_init(&mgr);  // Initialise event manager
  printf("Starting WS listener on %s/cardnr\n", s_listen_on);
  mg_http_listen(&mgr, s_listen_on, fn, NULL);  // Create HTTP listener
  for (;;) mg_mgr_poll(&mgr, 1000);             // Infinite event loop
  mg_mgr_free(&mgr);
  return 0;
}