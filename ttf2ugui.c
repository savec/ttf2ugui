/*
 * Copyright (c) 2015, Ari Suutari <ari@stonepile.fi>.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote
 *     products derived from this software without specific prior written
 *     permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT,  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <getopt.h>
#include <ft2build.h>
#include <wchar.h>
#include <locale.h>

#include FT_FREETYPE_H

#define SCREEN_WIDTH 132
#define SCREEN_HEIGHT 40
#include "ugui.h"

static UG_GUI gui;
static float fontSize = 0;
static int dpi = 0;

/*
 * "draw" a pixel using ansi escape sequences.
 * Used for printing a ascii art sample of font.
 */
static void drawPixel(UG_S16 x, UG_S16 y, UG_COLOR col)
{
  printf("\033[%d;%dH", y + 1, x + 1);
  if (col)
    printf(" ");
  else
    printf("*");
  fflush(stdout);
}

static int max(int a, int b)
{
  if (a > b)
    return a;
  return b;
}

static int min(int a, int b)
{
  if (a < b)
    return a;
  return b;
}

UG_RESULT searchUnicode(const UG_FONT * font, wchar_t *unicode, UG_U32 index)
{
  int i;
  for(i = 0; i < font->dict_size; i++)
    if(font->dict[i].index == index)
    {
      *unicode = font->dict[i].unicode;
      return UG_RESULT_OK;
    }
  return UG_RESULT_FAIL;
}

/*
 * Output C-language code that can be used to include 
 * converted font into uGUI application.
 */
static void dumpFont(const UG_FONT * font, const char* fontFile, float fontSize)
{
  int bytesPerChar;
  int ch;
  int current;
  int b;
  char fontName[80];
  char fileNameBuf[80];
  const char* baseName;
  char outFileName[80];
  char* ptr;
  FILE* out;

/*
 * Generate name for font by stripping path and suffix from filename.
 */
  baseName = fontFile;
  ptr = strrchr(baseName, '/');
  if (ptr)
    baseName = ptr + 1;

  strcpy(fileNameBuf, baseName);
  baseName = fileNameBuf;
  ptr = strchr(baseName, '.');
  if (ptr)
    *ptr = '\0';

  sprintf(fontName, "%s_%dX%d", baseName, font->char_width, font->char_height);
  sprintf(outFileName, "%s_%dX%d.c", baseName, font->char_width, font->char_height);
  out = fopen(outFileName, "w");
  if (!out) {

    perror(outFileName);
    exit(2);
  }

/*
 * First output character bitmaps.
 */
  bytesPerChar = font->char_height * (font->char_width / 8);
  if (font->char_width % 8)
    bytesPerChar += font->char_height;

  fprintf(out, "// Converted from %s\n", fontFile);
  fprintf(out, "//  --size %d\n", (int)fontSize);
  if (dpi > 0)
    fprintf(out, "//  --dpi %d\n", dpi);

  fprintf(out, "// For copyright, see original font file.\n");
  fprintf(out, "\n#include \"ugui.h\"\n\n");

  fprintf(out, "static __UG_FONT_DATA unsigned char fontBits_%s[%d][%d] = {\n", fontName, font->num_chars, bytesPerChar);

  current = 0;
  for (ch = 0; ch < font->num_chars; ch++) {

    fprintf(out, "  {");
    for (b = 0; b < bytesPerChar; b++) {

      if (b)
        fprintf(out, ",");

      fprintf(out, "0x%02X", font->p[current]);
      ++current;
    }

    fprintf(out, " }");
    if (ch <= font->num_chars - 1)
      fprintf(out, ", ");
    else
      fprintf(out, " ");
    
    wchar_t uc;
    if(searchUnicode(font, &uc, ch) == UG_RESULT_OK)
      fprintf(out, " // 0x%X", uc);
    fprintf(out, "\n");
  }

  fprintf(out, "};\n");

/*
 * Next output character widths.
 */
  fprintf(out, "static const UG_U8 fontWidths_%s[] = {\n", fontName);

  for (ch = 0; ch < font->num_chars; ch++) {

    if (ch != 0)
      fprintf(out, ", ");

    fprintf(out, "%d", font->widths[ch]);
  }

  fprintf(out, "};\n");

/*
 * Next output dictionary (unicode -> index).
 */
  fprintf(out, "static const UG_CHAR_CODE fontDict_%s[] = {\n", fontName);

  for (ch = 0; ch < font->dict_size; ch++) {

    if (ch != 0)
      fprintf(out, ", ");

    fprintf(out, "{%d, %u}", font->dict[ch].unicode, font->dict[ch].index);
  }

  fprintf(out, "};\n");

/*
 * Last, output UG_FONT structure.
 */
  fprintf(out, "const UG_FONT font_%s = { (unsigned char*)fontBits_%s, FONT_TYPE_1BPP, %d, %d, %d, fontWidths_%s, fontDict_%s, %d };\n",
          fontName,
          fontName,
          font->char_width,
          font->char_height,
          font->num_chars,
          fontName,
          fontName,
          font->dict_size
          );

  fclose(out);

  sprintf(outFileName, "%s_%dX%d.h", baseName, font->char_width, font->char_height);
  out = fopen(outFileName, "w");
  if (!out) {

    perror(outFileName);
    exit(2);
  }

/*
 * Output extern declaration to header file.
 */
  fprintf(out, "extern const UG_FONT font_%s;\n", fontName);
  fclose(out);
}

static UG_FONT newFont;

static UG_FONT *convertFont(const char *font, int dpi, float fontSize)
{
  int error;
  FT_Face face;
  FT_Library library;

/*
 * Initialize freetype library, load the font
 * and set output character size.
 */
  error = FT_Init_FreeType(&library);
  if (error) {

    fprintf(stderr, "ft init err %d\n", error);
    exit(1);
  }

  error = FT_New_Face(library,
                      font,
                      0,
                      &face);
  if (error) {

    fprintf(stderr, "ew faceerr %d\n", error);
    exit(1);
  }

/*
 * If DPI is not given, use pixes to specify the size.
 */
  if (dpi > 0)
    error = FT_Set_Char_Size(face, 0, fontSize * 64, dpi, dpi);
  else
    error = FT_Set_Pixel_Sizes(face, 0, fontSize);
  if (error) {

    fprintf(stderr, "set pixel sizes err %d\n", error);
    exit(1);
  }

  FT_ULong  charcode;
  FT_UInt   gindex;

  charcode = FT_Get_First_Char( face, &gindex );

  int minChar = INT_MAX;
  int maxChar = 0;
  int num_chars;
  // int ch;
  int maxWidth = 0;
  int maxHeight = 0;
  int maxAscent = 0;
  int maxDescent = 0;
  int bytesPerChar;

  int dict_size = 0;
  while ( gindex != 0 )
  {
    int ascent;
    int descent;

    error = FT_Load_Char(face, charcode, FT_LOAD_RENDER | FT_LOAD_TARGET_MONO);
    if (error) {

      fprintf(stderr, "load char err %d\n", error);
      exit(1);
    }

    descent = max(0, face->glyph->bitmap.rows - face->glyph->bitmap_top);
    ascent = max(0, max(face->glyph->bitmap_top, face->glyph->bitmap.rows) - descent);
    maxDescent = max(maxDescent, descent);
    maxAscent = max(maxAscent, ascent);
    maxWidth = max(face->glyph->bitmap.width, maxWidth);
    maxChar = max(maxChar, gindex);
    minChar = min(minChar, gindex);

    ++dict_size;
    charcode = FT_Get_Next_Char( face, charcode, &gindex );
  }

  num_chars = maxChar - minChar + 1;

/*
 * First found out how big character bitmap is needed. Every character
 * must fit into it so that we can obtain correct character positioning.
 */
  int bytesPerRow = maxWidth / 8;

  if (maxWidth % 8)
    ++bytesPerRow;

  maxHeight = maxAscent + maxDescent;
  bytesPerChar = bytesPerRow * maxHeight;

  newFont.p = malloc(bytesPerChar * num_chars);
  if(!newFont.p)
    perror("malloc\n");
  memset(newFont.p, '\0', bytesPerChar * num_chars);

  newFont.font_type = FONT_TYPE_1BPP;
  newFont.char_width = maxWidth;
  newFont.char_height = maxHeight;
  newFont.widths = malloc(num_chars);
  newFont.num_chars = num_chars;

  newFont.dict = malloc(sizeof(UG_CHAR_CODE)*dict_size);
  if(!newFont.dict)
    perror("malloc");
  memset(newFont.dict, 0, sizeof(UG_CHAR_CODE)*dict_size);
  newFont.dict_size = dict_size;

/*
 * Render each character.
 */
  UG_CHAR_CODE *pdict = newFont.dict;
  charcode = FT_Get_First_Char( face, &gindex );
  while ( gindex != 0 )
  {
    error = FT_Load_Char(face, charcode, FT_LOAD_RENDER | FT_LOAD_TARGET_MONO);
    if (error) {

      fprintf(stderr, "load char err %d\n", error);
      exit(1);
    }

    int i, j;
    for (i = 0; i < face->glyph->bitmap.rows; i++)
      for (j = 0; j < face->glyph->bitmap.width; j++) {

        uint8_t *bits = (uint8_t *) face->glyph->bitmap.buffer;
        uint8_t b = bits[i * face->glyph->bitmap.pitch + (j / 8)];

        int xpos, ypos;

/*
 * Output character to correct position in bitmap
 */
        xpos = j + face->glyph->bitmap_left;
        ypos = maxAscent + i - face->glyph->bitmap_top;

        int ind;

        ind = ypos * bytesPerRow;
        ind += xpos / 8;

        if (b & (1 << (7 - (j % 8))))
          newFont.p[((gindex - minChar) * bytesPerChar) + ind] |= (1 << ((xpos % 8)));

      }

    pdict->unicode = charcode;
    pdict->index = gindex - minChar;
    pdict ++;
/*
 * Save character width, freetype uses 1/64 as units for it.
 */
    newFont.widths[gindex - minChar] = face->glyph->advance.x >> 6;

    charcode = FT_Get_Next_Char( face, charcode, &gindex );
  }

  return &newFont;
}

/*
 * Draw a simple sample of new font with uGUI.
 */
static void showFont(const UG_FONT * font, wchar_t* text)
{
  UG_Init(&gui, drawPixel, SCREEN_WIDTH, SCREEN_HEIGHT);

  UG_FillScreen(C_WHITE);
  UG_DrawFrame(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, C_BLACK);
  UG_FontSelect(font);
  UG_SetBackcolor(C_WHITE);
  UG_SetForecolor(C_BLACK);
  UG_PutString(2, 2, text);
  UG_DrawPixel(0, SCREEN_HEIGHT - 1, C_WHITE);

  UG_Update();
  printf("\n");
}

static int dump, show;
static char* fontFile = NULL;

 /* options descriptor */
static struct option longopts[] = {
  {"show", no_argument, &show, 1},
  {"dump", no_argument, &dump, 1},
  {"dpi", required_argument, NULL, 'd'},
  {"size", required_argument, NULL, 's'},
  {"font", required_argument, NULL, 'f'},
  {NULL, 0, NULL, 0}
};

static void usage()
{
  fprintf(stderr, "ttf2ugui {--show|--dump} --font=fontfile [--dpi=displaydpi] --size=fontsize\n");
  fprintf(stderr, "If --dpi is not given, font size is assumed to be pixels.\n");
}

int main(int argc, char **argv)
{
  setlocale(LC_CTYPE, "");
  int ch;

  while ((ch = getopt_long(argc, argv, "", longopts, NULL)) != -1) {

    switch (ch) {
    case 'f':
      fontFile = optarg;
      break;

    case 's':
      sscanf(optarg, "%f", &fontSize);
      break;

    case 'd':
      dpi = atoi(optarg);
      break;

    case 0:
      break;

    default:
      usage();
      exit(1);
    }
  }

  if ((!dump && !show) || fontFile == NULL || fontSize == 0) {

    usage();
    exit(1);
  }

  const UG_FONT *font;

  font = convertFont(fontFile, dpi, fontSize);

  if (show)
  {
#define MAX_TEST_STR  256
    wchar_t test_str[MAX_TEST_STR];
    if(fgetws(test_str, MAX_TEST_STR, stdin) != NULL)
      showFont(font, test_str);
  }

  if (dump)
    dumpFont(font, fontFile, fontSize);
}
