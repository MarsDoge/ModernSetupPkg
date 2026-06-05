
#include "LvglUefiPort.h"

#define LVGL_HEAD_SIGNATURE  SIGNATURE_32('l','v','g','l')

typedef struct {
  UINT32    Signature;
  UINTN     Size;
} LVGL_HEAD;

#define LVGL_OVERHEAD  sizeof(LVGL_HEAD)

void* memset (void *dest, char ch, unsigned int count)
{
  return SetMem (dest, count, ch);
}


void *
malloc (
  size_t  size
  )
{
  LVGL_HEAD  *PoolHdr;
  UINTN         NewSize;
  VOID          *Data;

  NewSize = (UINTN)(size) + LVGL_OVERHEAD;

  Data = AllocatePool (NewSize);
  if (Data != NULL) {
    PoolHdr            = (LVGL_HEAD *)Data;
    PoolHdr->Signature = LVGL_HEAD_SIGNATURE;
    PoolHdr->Size      = size;

    return (VOID *)(PoolHdr + 1);
  }

  return NULL;
}

void *
realloc (
  void    *ptr,
  size_t  size
  )
{
  LVGL_HEAD  *OldPoolHdr;
  LVGL_HEAD  *NewPoolHdr;
  UINTN         OldSize;
  UINTN         NewSize;
  VOID          *Data;

  NewSize = (UINTN)size + LVGL_OVERHEAD;
  Data    = AllocatePool (NewSize);
  if (Data != NULL) {
    NewPoolHdr            = (LVGL_HEAD *)Data;
    NewPoolHdr->Signature = LVGL_HEAD_SIGNATURE;
    NewPoolHdr->Size      = size;
    if (ptr != NULL) {
      OldPoolHdr = (LVGL_HEAD *)ptr - 1;
      ASSERT (OldPoolHdr->Signature == LVGL_HEAD_SIGNATURE);
      OldSize = OldPoolHdr->Size;

      CopyMem ((VOID *)(NewPoolHdr + 1), ptr, MIN (OldSize, size));
      FreePool ((VOID *)OldPoolHdr);
    }

    return (VOID *)(NewPoolHdr + 1);
  }

  return NULL;
}

void
free (
  void  *ptr
  )
{
  VOID          *EvalOnce;
  LVGL_HEAD  *PoolHdr;

  EvalOnce = ptr;
  if (EvalOnce == NULL) {
    return;
  }

  PoolHdr = (LVGL_HEAD *)EvalOnce - 1;
  if (PoolHdr->Signature == LVGL_HEAD_SIGNATURE) {
    FreePool (PoolHdr);
  } else {
    FreePool (EvalOnce);
  }
}


/* Return the absolute value of I.  */
long int
labs (long int i)
{
  return i < 0 ? -i : i;
}

/* Return the absolute value of I.  */
int
abs (int i)
{
  return i < 0 ? -i : i;
}

char *strchr(const char *str, int ch)
{
  return ScanMem8 (str, AsciiStrSize (str), (UINT8)ch);
}


char *
strcpy (
  char        *strDest,
  const char  *strSource
  )
{
  AsciiStrCpyS (strDest, AsciiStrnSizeS (strSource, MAX_STRING_SIZE - 1), strSource);
  return strDest;
}

char *
strncpy (
  char        *strDest,
  const char  *strSource,
  size_t      count
  )
{
  UINTN  DestMax = MAX_STRING_SIZE;

  if (count < MAX_STRING_SIZE) {
    DestMax = count + 1;
  } else {
    count = MAX_STRING_SIZE-1;
  }

  AsciiStrnCpyS (strDest, DestMax, strSource, (UINTN)count);

  return strDest;
}

char *
strcat (
  char        *strDest,
  const char  *strSource
  )
{
  UINTN  DestMax;

  DestMax = AsciiStrnLenS (strDest, MAX_STRING_SIZE) + AsciiStrnSizeS (strSource, MAX_STRING_SIZE);

  if (DestMax > MAX_STRING_SIZE) {
    DestMax = MAX_STRING_SIZE;
  }

  AsciiStrCatS (strDest, DestMax, strSource);

  return strDest;
}


char *
strncat (
  char        *strDest,
  const char  *strSource,
  size_t      count
  )
{
  UINTN  DestMax = MAX_STRING_SIZE;

  DestMax = AsciiStrnLenS (strDest, MAX_STRING_SIZE) + AsciiStrnSizeS (strSource, MAX_STRING_SIZE);

  if (DestMax > MAX_STRING_SIZE) {
    DestMax = MAX_STRING_SIZE;
  }

  AsciiStrnCatS (strDest, DestMax, strSource, (UINTN)count);

  return strDest;
}

FILE *
fopen (
  const char  *filename,
  const char  *mode
  )
{
  return NULL;
}

int
fclose (
  FILE  *stream
  )
{
  return EOF;
}

int
fscanf      (
  FILE *stream,
  const char *format,
  ...
  )
{
  return EOF;
}
