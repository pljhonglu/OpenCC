/*
 * Open Chinese Convert
 *
 * Copyright 2010-2013 BYVoid <byvoid@byvoid.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "common.h"
#include "config_reader.h"
#include "converter.h"
#include "dict_group.h"
#include "dict_chain.h"
#include "encoding.h"
#include "opencc.h"

typedef struct {
  DictChain* DictChain;
  converter_t converter;
} opencc_desc;

static opencc_error errnum = OPENCC_ERROR_VOID;
static int lib_initialized = FALSE;

static void lib_initialize(void) {
#ifdef ENABLE_GETTEXT
  bindtextdomain(PACKAGE_NAME, LOCALEDIR);
#endif /* ifdef ENABLE_GETTEXT */
  lib_initialized = TRUE;
}

size_t opencc_convert(opencc_t t_opencc,
                      ucs4_t** inbuf,
                      size_t* inbuf_left,
                      ucs4_t** outbuf,
                      size_t* outbuf_left) {
  if (!lib_initialized) {
    lib_initialize();
  }
  opencc_desc* opencc = (opencc_desc*)t_opencc;
  size_t retval = converter_convert
                    (opencc->converter, inbuf, inbuf_left, outbuf, outbuf_left);
  if (retval == (size_t)-1) {
    errnum = OPENCC_ERROR_CONVERTER;
  }
  return retval;
}

char* opencc_convert_utf8(opencc_t t_opencc, const char* inbuf, size_t length) {
  if (!lib_initialized) {
    lib_initialize();
  }
	size_t actual_length = strlen(inbuf);
  if ((length == (size_t)-1) || (length > actual_length)) {
    length = actual_length;
  }
  ucs4_t* winbuf = utf8_to_ucs4(inbuf, length);
  if (winbuf == (ucs4_t*)-1) {
    /* Can not convert input UTF8 to UCS4 */
    errnum = OPENCC_ERROR_ENCODING;
    return (char*)-1;
  }
  /* Set up UTF8 buffer */
  size_t outbuf_len = length;
  size_t outsize = outbuf_len;
  char* original_outbuf = (char*)malloc(sizeof(char) * (outbuf_len + 1));
  char* outbuf = original_outbuf;
  original_outbuf[0] = '\0';
  /* Set conversion buffer */
  size_t wbufsize = length + 64;
  ucs4_t* woutbuf = (ucs4_t*)malloc(sizeof(ucs4_t) * (wbufsize + 1));
  ucs4_t* pinbuf = winbuf;
  ucs4_t* poutbuf = woutbuf;
  size_t inbuf_left, outbuf_left;
  inbuf_left = ucs4len(winbuf);
  outbuf_left = wbufsize;
  while (inbuf_left > 0) {
    size_t retval = opencc_convert(t_opencc,
                                   &pinbuf,
                                   &inbuf_left,
                                   &poutbuf,
                                   &outbuf_left);
    if (retval == (size_t)-1) {
      free(outbuf);
      free(winbuf);
      free(woutbuf);
      return (char*)-1;
    }
    *poutbuf = L'\0';
    char* ubuff = ucs4_to_utf8(woutbuf, (size_t)-1);
    if (ubuff == (char*)-1) {
      free(outbuf);
      free(winbuf);
      free(woutbuf);
      errnum = OPENCC_ERROR_ENCODING;
      return (char*)-1;
    }
    size_t ubuff_len = strlen(ubuff);
    while (ubuff_len > outsize) {
      size_t outbuf_offset = outbuf - original_outbuf;
      outsize += outbuf_len;
      outbuf_len += outbuf_len;
      original_outbuf =
        (char*)realloc(original_outbuf, sizeof(char) * outbuf_len);
      outbuf = original_outbuf + outbuf_offset;
    }
    strncpy(outbuf, ubuff, ubuff_len);
    free(ubuff);
    outbuf += ubuff_len;
    *outbuf = '\0';
    outbuf_left = wbufsize;
    poutbuf = woutbuf;
  }
  free(winbuf);
  free(woutbuf);
  original_outbuf = (char*)realloc(original_outbuf,
                                   sizeof(char) * (strlen(original_outbuf) + 1));
  return original_outbuf;
}

opencc_t opencc_open(const char* config_file) {
  if (!lib_initialized) {
    lib_initialize();
  }
  opencc_desc* opencc;
  opencc = (opencc_desc*)malloc(sizeof(opencc_desc));
  opencc->DictChain = NULL;
  opencc->converter = converter_open();
  converter_set_conversion_mode(opencc->converter, OPENCC_CONVERSION_FAST);
  if (config_file == NULL) {
    /* TODO load default */
    assert(0);
  } else {
    /* Load config */
    config_t config = config_open(config_file);
    if (config == (config_t)-1) {
      errnum = OPENCC_ERROR_CONFIG;
      return (opencc_t)-1;
    }
    opencc->DictChain = config_get_DictChain(config);
    converter_assign_dictionary(opencc->converter, opencc->DictChain);
    config_close(config);
  }
  return (opencc_t)opencc;
}

int opencc_close(opencc_t t_opencc) {
  if (!lib_initialized) {
    lib_initialize();
  }
  opencc_desc* opencc = (opencc_desc*)t_opencc;
  converter_close(opencc->converter);
  if (opencc->DictChain != NULL) {
    DictChain_close(opencc->DictChain);
  }
  free(opencc);
  return 0;
}

int opencc_dict_load(opencc_t t_opencc,
                     const char* dict_filename,
                     opencc_dictionary_type dict_type) {
  if (!lib_initialized) {
    lib_initialize();
  }
  opencc_desc* opencc = (opencc_desc*)t_opencc;
  DictGroup_t DictGroup;
  if (opencc->DictChain == NULL) {
    opencc->DictChain = DictChain_open(NULL);
    DictGroup = DictChain_new_group(opencc->DictChain);
  } else {
    DictGroup = DictChain_get_group(opencc->DictChain, 0);
  }
  int retval = DictGroup_load(DictGroup, dict_filename, dict_type);
  if (retval == -1) {
    errnum = OPENCC_ERROR_DICTLOAD;
    return -1;
  }
  converter_assign_dictionary(opencc->converter, opencc->DictChain);
  return retval;
}

void opencc_set_conversion_mode(opencc_t t_opencc,
                                opencc_conversion_mode conversion_mode) {
  if (!lib_initialized) {
    lib_initialize();
  }
  opencc_desc* opencc = (opencc_desc*)t_opencc;
  converter_set_conversion_mode(opencc->converter, conversion_mode);
}

opencc_error opencc_errno(void) {
  if (!lib_initialized) {
    lib_initialize();
  }
  return errnum;
}

void opencc_perror(const char* spec) {
  if (!lib_initialized) {
    lib_initialize();
  }
  perr(spec);
  perr("\n");
  switch (errnum) {
  case OPENCC_ERROR_VOID:
    break;
  case OPENCC_ERROR_DICTLOAD:
    dictionary_perror(_("Dictionary loading error"));
    break;
  case OPENCC_ERROR_CONFIG:
    config_perror(_("Configuration error"));
    break;
  case OPENCC_ERROR_CONVERTER:
    converter_perror(_("Converter error"));
    break;
  case OPENCC_ERROR_ENCODING:
    perr(_("Encoding error"));
    break;
  default:
    perr(_("Unknown"));
  }
  perr("\n");
}
