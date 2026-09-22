/* Host execution uses the real generated Setup.h and shipped ReviewRuntime.inc. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#undef NULL
#include "Setup.h"
STATIC BOOLEAN failSetter;
BOOLEAN EFIAPI DebugAssertEnabled(VOID) { return !failSetter; }
VOID EFIAPI DebugAssert(CONST CHAR8 *file, UINTN line, CONST CHAR8 *desc) { fprintf(stderr,"ASSERT %s:%lu %s\n",file,line,desc);abort(); }
#include "ReviewRuntime.inc"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
LIST_ENTRY gBrowserFormSetList;
UINTN gBrowserContextCount = 1;
STATIC FORM_BROWSER_FORMSET *invalidFormSet;
UI_MENU_SELECTION *gCurrentSelection;
BOOLEAN mHiiPackageListUpdated;
EFI_BOOT_SERVICES *gBS;
VOID *EFIAPI AllocateZeroPool(UINTN n) { return calloc(1,n); }
VOID *EFIAPI AllocateCopyPool(UINTN n, CONST VOID *p) { if(failSetter && n==sizeof(L"1234") && StrCmp(p,L"1234")==0)return NULL; VOID *v=malloc(n); if(v) memcpy(v,p,n); return v; }
VOID EFIAPI FreePool(VOID *p) { free(p); }
VOID *EFIAPI ZeroMem(VOID *p, UINTN n) { return memset(p,0,n); }
VOID *EFIAPI SetMem(VOID *p, UINTN n, UINT8 v) { return memset(p,v,n); }
VOID *EFIAPI CopyMem(VOID *d, CONST VOID *s, UINTN n) { return memcpy(d,s,n); }
INTN EFIAPI CompareMem(CONST VOID *a, CONST VOID *b, UINTN n) { return memcmp(a,b,n); }
EFI_GUID *EFIAPI CopyGuid(EFI_GUID *a, CONST EFI_GUID *b) { memcpy(a,b,sizeof(*a)); return a; }
UINTN EFIAPI StrLen(CONST CHAR16 *s) { UINTN n=0; while(s[n]) n++; return n; }
UINTN EFIAPI StrSize(CONST CHAR16 *s) { return (StrLen(s)+1)*2; }
INTN EFIAPI StrnCmp(CONST CHAR16 *a, CONST CHAR16 *b, UINTN n) { while(n--){if(*a!=*b || !*a)return *a-*b; a++; b++;}return 0; }
INTN EFIAPI StrCmp(CONST CHAR16 *a, CONST CHAR16 *b) { while(*a&&*a==*b){a++;b++;}return *a-*b; }
CHAR16 *EFIAPI StrStr(CONST CHAR16 *a, CONST CHAR16 *b) { UINTN n=StrLen(b); for(;*a;a++)if(!StrnCmp(a,b,n))return (CHAR16*)a;return NULL; }
RETURN_STATUS EFIAPI StrHexToUintnS(CONST CHAR16 *s, CHAR16 **end, UINTN *v) { *v=0; while((*s>='0'&&*s<='9')||(*s>='a'&&*s<='f')||(*s>='A'&&*s<='F')) { UINTN d=*s<='9'?*s-'0':(*s|32)-'a'+10; if(*v>(MAX_UINTN-d)/16)return RETURN_UNSUPPORTED; *v=*v*16+d; s++; } *end=(CHAR16*)s;return 0; }
LIST_ENTRY *EFIAPI InitializeListHead(LIST_ENTRY *h) { h->ForwardLink=h->BackLink=h; return h; }
LIST_ENTRY *EFIAPI InsertTailList(LIST_ENTRY *h, LIST_ENTRY *n) { n->BackLink=h->BackLink;n->ForwardLink=h;h->BackLink->ForwardLink=n;h->BackLink=n;return h; }
LIST_ENTRY *EFIAPI GetFirstNode(CONST LIST_ENTRY *h) { return h->ForwardLink; }
LIST_ENTRY *EFIAPI GetNextNode(CONST LIST_ENTRY *h, CONST LIST_ENTRY *n) { (void)h;return n->ForwardLink; }
BOOLEAN EFIAPI IsNull(CONST LIST_ENTRY *h, CONST LIST_ENTRY *n) { return h==n; }
CHAR16 *EFIAPI HiiGetString(EFI_HII_HANDLE h, EFI_STRING_ID id, CONST CHAR8 *lang) { (void)h;(void)lang; CONST CHAR16 *s=id==1?L"BMM2":id==2?L"BootNext":id==3?L"Timeout":L"Protected";return AllocateCopyPool(StrSize(s),s); }
UINTN EFIAPI UnicodeSPrint(CHAR16 *out, UINTN size, CONST CHAR16 *fmt, ...) { va_list ap; UINTN n=0;va_start(ap,fmt); if(fmt[1]=='L') { UINT64 v=va_arg(ap,UINT64);CHAR16 rev[32];UINTN k=0;do{rev[k++]='0'+v%10;v/=10;}while(v);while(k&&n+1<size/2)out[n++]=rev[--k]; } else if(fmt[2]=='&') { out[0]='G';out[1]=0;va_end(ap);return 1; } else { CHAR16 *a=va_arg(ap,CHAR16*),*b=va_arg(ap,CHAR16*);while(*a&&n+1<size/2)out[n++]=*a++;if(n+4<size/2){out[n++]=' ';out[n++]='/';out[n++]=' ';}while(*b&&n+1<size/2)out[n++]=*b++; }out[n]=0;va_end(ap);return n; }
BOOLEAN IsNvUpdateRequiredForForm(FORM_BROWSER_FORM *f) { (void)f;return TRUE; }
BOOLEAN IsNvUpdateRequiredForFormSet(FORM_BROWSER_FORMSET *f) { (void)f;return TRUE; }
BOOLEAN ValidateFormSet(FORM_BROWSER_FORMSET *f) { if(f==invalidFormSet){f->Link.BackLink->ForwardLink=f->Link.ForwardLink;f->Link.ForwardLink->BackLink=f->Link.BackLink; free(f);invalidFormSet=NULL;return FALSE;}return TRUE; }
STATIC EFI_STATUS ReviewOnly(FORM_BROWSER_FORMSET *fs, FORM_BROWSER_FORM *f, BROWSER_SETTING_SCOPE scope) { EFI_STATUS s=ModernReviewSubmit(fs,f,scope);ModernReviewFinish();return s; }
#define ModernReviewSubmit ReviewOnly
STATIC BROWSER_STORAGE store;
STATIC UINT8 before[32],after[32];
STATIC UINTN expectedCount,calls;
STATIC BOOLEAN expectedComplete=TRUE;
STATIC int mode;
STATIC MODERN_SETUP_REVIEW_SCOPE expectedScope;
STATIC EFI_STATUS EFIAPI review(MODERN_SETUP_CHANGE_REVIEW_PROTOCOL *ui, CONST MODERN_SETUP_REVIEW_SNAPSHOT *s, MODERN_SETUP_REVIEW_DECISION *d) {
  (void)ui;calls++;CHECK(s->ItemCount==expectedCount);CHECK(s->Complete==expectedComplete);CHECK(s->Scope==expectedScope);
  for(UINTN i=0;i<s->ItemCount;i++) { CHECK(s->Items[i].Path); if(s->Items[i].Flags&MODERN_SETUP_REVIEW_REDACTED) { CHECK(!s->Items[i].OldValue&&!s->Items[i].NewValue); } else { CHECK(s->Items[i].OldValue&&s->Items[i].NewValue); } }
  *d=mode==1?ModernSetupContinueEditing:ModernSetupConfirmSave;
  if(mode==2)after[0]++;
  if(mode==3)mHiiPackageListUpdated=TRUE;
  return EFI_SUCCESS;
}
STATIC MODERN_SETUP_CHANGE_REVIEW_PROTOCOL ui={MODERN_SETUP_CHANGE_REVIEW_REVISION,review};
STATIC EFI_STATUS EFIAPI locate(EFI_GUID *g, VOID *r, VOID **p) {  if(mode==4)return EFI_NOT_FOUND;
  (void)g;(void)r;*p=&ui;return EFI_SUCCESS; }
BROWSER_SETTING_SCOPE gBrowserSettingScope=FormSetLevel;
BOOLEAN gResetRequiredFormLevel,gResetRequiredSystemLevel;
EXIT_HANDLER ExitHandlerFunction;
EFI_RUNTIME_SERVICES *gRT;
EFI_HII_CONFIG_ROUTING_PROTOCOL *mHiiConfigRouting;
STATIC int discarded,exited,defaults;
STATIC EFI_STATUS extractStatus,convertStatus,syncStatus,submitStatus=EFI_ABORTED;
STATIC BOOLEAN skipSync;
BOOLEAN FindNextMenu(UI_MENU_SELECTION *s, BROWSER_SETTING_SCOPE scope) { (void)s;(void)scope;exited++;return TRUE; }
EFI_STATUS DiscardForm(FORM_BROWSER_FORMSET *fs, FORM_BROWSER_FORM *f, BROWSER_SETTING_SCOPE scope) { (void)fs;(void)f;(void)scope;discarded++;return EFI_SUCCESS; }
EFI_STATUS SubmitForm(FORM_BROWSER_FORMSET *fs, FORM_BROWSER_FORM *f, BROWSER_SETTING_SCOPE scope) { (void)fs;(void)f;(void)scope;return submitStatus; }
VOID UpdateStatementStatus(FORM_BROWSER_FORMSET *fs, FORM_BROWSER_FORM *f, BROWSER_SETTING_SCOPE scope) { (void)fs;(void)f;(void)scope; }
EFI_STATUS ExtractDefault(FORM_BROWSER_FORMSET *fs, FORM_BROWSER_FORM *f, UINT16 id, BROWSER_SETTING_SCOPE scope, BROWSER_GET_DEFAULT_VALUE v, BROWSER_STORAGE *s, BOOLEAN a, BOOLEAN b) { (void)fs;(void)f;(void)id;(void)scope;(void)v;(void)s;(void)a;(void)b;defaults++;return EFI_SUCCESS; }
BOOLEAN ConfigRequestAdjust(BROWSER_STORAGE *s, CHAR16 *r, BOOLEAN b) { (void)s;(void)r;(void)b;return FALSE; }
EFI_STATUS SynchronizeStorage(BROWSER_STORAGE *s, CHAR16 *r, BOOLEAN b) { (void)r;(void)b;if(skipSync)return EFI_SUCCESS;if(!EFI_ERROR(syncStatus)){if(s->Type==EFI_HII_VARSTORE_NAME_VALUE){for(LIST_ENTRY *l=GetFirstNode(&s->NameValueListHead);!IsNull(&s->NameValueListHead,l);l=GetNextNode(&s->NameValueListHead,l)){NAME_VALUE_NODE *node=NAME_VALUE_NODE_FROM_LINK(l);FreePool(node->Value);CHECK(node->EditValue!=NULL);node->Value=AllocateCopyPool(StrSize(node->EditValue),node->EditValue);}}else if(s->Size)CopyMem(s->Buffer,s->EditBuffer,s->Size);}return syncStatus; }
STATIC EFI_STATUS EFIAPI configToBlock(CONST EFI_HII_CONFIG_ROUTING_PROTOCOL *p, CONST EFI_STRING r, UINT8 *b, UINTN *n, EFI_STRING *progress) { (void)p;(void)r;(void)b;(void)n;(void)progress;return convertStatus; }
STATIC CONST CHAR16 *extractResponse=L"GUID=x&OFFSET=0&WIDTH=20&VALUE=0000000000000000000000000000000000000000000000000000000000000000";
STATIC BROWSER_STORAGE *serializeFailStore;
EFI_STATUS StorageToConfigResp(BROWSER_STORAGE *s, EFI_STRING *resp, CHAR16 *request, BOOLEAN edit) {
  (void)edit;if(s==serializeFailStore)return EFI_OUT_OF_RESOURCES;
  UINTN n=StrLen(request);*resp=AllocateZeroPool((n+s->Size*2+8)*2);CopyMem(*resp,request,n*2);
  for(UINTN i=0;i<s->Size;i++){(*resp)[n++]=L"0123456789abcdef"[s->EditBuffer[i]>>4];(*resp)[n++]=L"0123456789abcdef"[s->EditBuffer[i]&15];}return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI extract(CONST EFI_HII_CONFIG_ROUTING_PROTOCOL *this, CONST EFI_STRING req, EFI_STRING *progress, EFI_STRING *result) { (void)this;(void)req;(void)progress;if(!EFI_ERROR(extractStatus))*result=extractResponse?AllocateCopyPool(StrSize(extractResponse),extractResponse):NULL;return extractStatus; }
LIST_ENTRY gBrowserSaveFailFormSetList;
BOOLEAN gCallbackReconnect,gFlagReconnect;
BOOLEAN EFIAPI IsListEmpty(CONST LIST_ENTRY *h){return h->ForwardLink==h;}
LIST_ENTRY *EFIAPI RemoveEntryList(CONST LIST_ENTRY *n){n->BackLink->ForwardLink=n->ForwardLink;n->ForwardLink->BackLink=n->BackLink;return n->ForwardLink;}
LIST_ENTRY *EFIAPI InsertHeadList(LIST_ENTRY *h,LIST_ENTRY *n){n->ForwardLink=h->ForwardLink;n->BackLink=h;h->ForwardLink->BackLink=n;h->ForwardLink=n;return h;}
EFI_STATUS NoSubmitCheck(FORM_BROWSER_FORMSET *fs,FORM_BROWSER_FORM **f,FORM_BROWSER_STATEMENT **q){(void)fs;(void)f;(void)q;return EFI_SUCCESS;}
VOID GetSyncRestoreConfigRequest(BROWSER_STORAGE *s,CHAR16 *r,CHAR16 *p,CHAR16 **restore,CHAR16 **sync){(void)s;(void)r;(void)p;*restore=AllocateCopyPool(sizeof(L"pending"),L"pending");*sync=NULL;}
VOID ValueChangeResetFlagUpdate(BOOLEAN a,FORM_BROWSER_FORMSET *fs,FORM_BROWSER_FORM *f){(void)a;(void)fs;(void)f;}
STATIC int submitted,routeCalls,routeMode;
STATIC BROWSER_STORAGE *otherStore;
VOID SubmitCallback(FORM_BROWSER_FORMSET *fs,FORM_BROWSER_FORM *f){(void)fs;(void)f;submitted++;}
VOID FindQuestionFromProgress(FORM_BROWSER_FORMSET *fs,BROWSER_STORAGE *s,CHAR16 *p,FORM_BROWSER_FORM **f,FORM_BROWSER_STATEMENT **q){(void)fs;(void)s;(void)p;*f=NULL;*q=NULL;}
VOID SendDiscardInfoToDriver(FORM_BROWSER_FORMSET *fs,FORM_BROWSER_FORM *f){(void)fs;(void)f;discarded++;}
STATIC EFI_STATUS EFIAPI route(CONST EFI_HII_CONFIG_ROUTING_PROTOCOL *this,CONST EFI_STRING resp,EFI_STRING *progress){(void)this;routeCalls++;*progress=resp;if(routeCalls==1&&routeMode==1)otherStore->EditBuffer[0]++;if(routeCalls==1&&routeMode==2){serializeFailStore=otherStore;return EFI_DEVICE_ERROR;}return EFI_SUCCESS;}
#include "HostNative.inc"
#include "HostCallback.inc"
int main(void) {
  FORM_BROWSER_FORMSET fs={0};FORM_BROWSER_FORM form={0};FORM_BROWSER_STATEMENT q[4]={{0}};FORMSET_STORAGE ss={0};FORM_BROWSER_CONFIG_REQUEST req={0};EFI_BOOT_SERVICES bs={0};UI_MENU_SELECTION sel={0};
  fs.Signature=FORM_BROWSER_FORMSET_SIGNATURE;form.Signature=FORM_BROWSER_FORM_SIGNATURE;ss.Signature=FORMSET_STORAGE_SIGNATURE;req.Signature=FORM_BROWSER_CONFIG_REQUEST_SIGNATURE;
  InitializeListHead(&gBrowserFormSetList);InitializeListHead(&fs.FormListHead);InitializeListHead(&fs.StorageListHead);InitializeListHead(&form.StatementListHead);InitializeListHead(&form.ConfigRequestHead);
  InsertTailList(&gBrowserFormSetList,&fs.Link);InsertTailList(&fs.FormListHead,&form.Link);InsertTailList(&fs.StorageListHead,&ss.Link);InsertTailList(&form.ConfigRequestHead,&req.Link);
  form.FormTitle=1;form.FormId=1;store.Type=EFI_HII_VARSTORE_BUFFER;store.Size=sizeof(before);store.Buffer=before;store.EditBuffer=after;store.Initialized=TRUE;store.ModernReviewBaselineKnown=TRUE;
  ss.BrowserStorage=&store;ss.ElementCount=1;ss.ConfigRequest=L"GUID=x&OFFSET=0&WIDTH=20";req.Storage=&store;req.ElementCount=1;req.ConfigRequest=L"GUID=x&OFFSET=0&WIDTH=2";
  for(int i=0;i<4;i++){q[i].Signature=FORM_BROWSER_STATEMENT_SIGNATURE;q[i].Storage=&store;q[i].StorageWidth=2;q[i].VarStoreInfo.VarOffset=i*2;q[i].QuestionId=i+1;q[i].Prompt=i+2;q[i].Operand=EFI_IFR_NUMERIC_OP;InitializeListHead(&q[i].OptionListHead);InsertTailList(&form.StatementListHead,&q[i].Link);}
  q[2].Operand=EFI_IFR_PASSWORD_OP;q[3].Operand=EFI_IFR_ORDERED_LIST_OP;
  bs.LocateProtocol=locate;gBS=&bs;gCurrentSelection=&sel;
  before[0]=1;after[0]=2;before[2]=5;after[2]=10;
  expectedScope=ModernSetupReviewFormSet;expectedCount=2;
  CHECK(ModernReviewSubmit(&fs,&form,FormSetLevel)==EFI_SUCCESS);CHECK(calls==1);CHECK(before[0]==1&&after[0]==2);
  expectedScope=ModernSetupReviewForm;expectedCount=1;CHECK(ModernReviewSubmit(&fs,&form,FormLevel)==EFI_SUCCESS);
  expectedScope=ModernSetupReviewSystem;expectedCount=2;CHECK(ModernReviewSubmit(NULL,NULL,SystemLevel)==EFI_SUCCESS);
  expectedScope=ModernSetupReviewFormSet;mode=1;CHECK(ModernReviewSubmit(&fs,&form,FormSetLevel)==EFI_ABORTED);CHECK(after[2]==10);
  mode=2;CHECK(ModernReviewSubmit(&fs,&form,FormSetLevel)==EFI_ABORTED);after[0]=2;
  mode=3;CHECK(ModernReviewSubmit(&fs,&form,FormSetLevel)==EFI_ABORTED);mHiiPackageListUpdated=FALSE;mode=0;
  store.ModernReviewBaselineKnown=FALSE;expectedCount=1;expectedComplete=FALSE;CHECK(ModernReviewSubmit(&fs,&form,FormSetLevel)==EFI_ABORTED);store.ModernReviewBaselineKnown=TRUE;expectedComplete=TRUE;
  after[4]=42;after[6]=9;expectedCount=4;CHECK(ModernReviewSubmit(&fs,&form,FormSetLevel)==EFI_SUCCESS);
  after[20]=7;expectedCount=5;CHECK(ModernReviewSubmit(&fs,&form,FormSetLevel)==EFI_SUCCESS);
  memcpy(after,before,sizeof(after));UINTN prev=calls;CHECK(ModernReviewSubmit(&fs,&form,FormSetLevel)==EFI_SUCCESS);CHECK(calls==prev);
  CHECK(ModernReviewSubmit(&fs,&form,99)==EFI_UNSUPPORTED);
  // Bounded UTF-16 and ordered list old/new rendering, actual option labels.
  q[3].MaxContainers=2;before[6]=1;before[7]=2;after[6]=2;after[7]=1;
  QUESTION_OPTION options[2]={{0}};
  for(int i=0;i<2;i++){options[i].Signature=QUESTION_OPTION_SIGNATURE;options[i].Value.Value.u8=i+1;options[i].Text=i+2;InsertTailList(&q[3].OptionListHead,&options[i].Link);}
  CHAR16 *v=ReviewValue(&fs,&q[3],before);CHECK(!StrnCmp(v,L"BootNext > Timeout",19));FreePool(v);
  v=ReviewValue(&fs,&q[3],after);CHECK(!StrnCmp(v,L"Timeout > BootNext",19));FreePool(v);
  q[2].Operand=EFI_IFR_STRING_OP;before[4]='A';after[4]='B';v=ReviewValue(&fs,&q[2],after);CHECK(v[0]=='B'&&v[1]==0);FreePool(v);
  expectedCount=2;CHECK(ModernReviewSubmit(&fs,&form,FormSetLevel)==EFI_SUCCESS);
  // Overlapping password alias forces redaction of the public string.
  q[1].VarStoreInfo.VarOffset=4;q[1].Operand=EFI_IFR_PASSWORD_OP;CHECK(!ReviewAliasesSafe(&store,&q[2]));q[1].VarStoreInfo.VarOffset=2;
  ui.Revision=99;CHECK(ModernReviewSubmit(&fs,&form,FormSetLevel)==EFI_UNSUPPORTED);ui.Revision=MODERN_SETUP_CHANGE_REVIEW_REVISION;
  mode=4;CHECK(ModernReviewSubmit(&fs,&form,FormSetLevel)==EFI_UNSUPPORTED);mode=0;

  // Original CancelGuards ProcessAction runs, rather than token assertions.
  CHECK(ProcessAction(BROWSER_ACTION_SUBMIT|BROWSER_ACTION_EXIT|BROWSER_ACTION_RESET,0)==EFI_SUCCESS);CHECK(!discarded&&!exited&&!gResetRequiredFormLevel&&!gResetRequiredSystemLevel);CHECK(sel.Action==UI_ACTION_REFRESH_FORM);
  CHECK(ProcessAction(BROWSER_ACTION_SUBMIT|BROWSER_ACTION_DISCARD,0)==EFI_SUCCESS);CHECK(!discarded);
  CHECK(ProcessAction(BROWSER_ACTION_SUBMIT|BROWSER_ACTION_FORM_EXIT,0)==EFI_SUCCESS);CHECK(!exited);
  // Execute transformed native LoadStorage: failed extract/defaults is not trust.
  EFI_HII_CONFIG_ROUTING_PROTOCOL routing={0};routing.ExtractConfig=extract;routing.ConfigToBlock=configToBlock;mHiiConfigRouting=&routing;
  ss.ConfigHdr=L"GUID=x";store.Initialized=FALSE;extractStatus=EFI_DEVICE_ERROR;LoadStorage(&fs,&ss);CHECK(store.Initialized&&!store.ModernReviewBaselineKnown&&defaults==1);FreePool(store.ConfigRequest);store.ConfigRequest=NULL;
  store.Initialized=FALSE;extractStatus=EFI_SUCCESS;convertStatus=EFI_DEVICE_ERROR;LoadStorage(&fs,&ss);CHECK(!store.ModernReviewBaselineKnown);FreePool(store.ConfigRequest);store.ConfigRequest=NULL;
  store.Initialized=FALSE;convertStatus=EFI_SUCCESS;syncStatus=EFI_DEVICE_ERROR;LoadStorage(&fs,&ss);CHECK(!store.ModernReviewBaselineKnown);FreePool(store.ConfigRequest);store.ConfigRequest=NULL;
  store.Initialized=FALSE;syncStatus=EFI_SUCCESS;LoadStorage(&fs,&ss);CHECK(store.ModernReviewBaselineKnown);FreePool(store.ConfigRequest);store.ConfigRequest=NULL;
  // Successful extraction with a hole (even with maximum offset reached) is unknown.
  extractResponse=L"GUID=x&OFFSET=0&WIDTH=1&VALUE=00&OFFSET=1f&WIDTH=1&VALUE=00";
  store.Initialized=FALSE;LoadStorage(&fs,&ss);CHECK(!store.ModernReviewBaselineKnown);FreePool(store.ConfigRequest);store.ConfigRequest=NULL;
  extractResponse=L"GUID=x&OFFSET=0&WIDTH=20&VALUE=00";
  store.Initialized=FALSE;LoadStorage(&fs,&ss);CHECK(!store.ModernReviewBaselineKnown);FreePool(store.ConfigRequest);store.ConfigRequest=NULL;
  BROWSER_STORAGE small=store;small.Size=2;
  CHECK(ModernReviewFullCoverage(&small,L"GUID=x&OFFSET=1&WIDTH=1&VALUE=12&OFFSET=0&WIDTH=1&VALUE=34"));
  CHECK(!ModernReviewFullCoverage(&small,L"GUID=x&OFFSET=0&WIDTH=1&VALUE=12&OFFSET=0&WIDTH=1&VALUE=34"));
  store.ModernReviewBaselineKnown=TRUE;
  // ValidateFormSet actually unlinks and frees this node; traversal must prefetch.
  invalidFormSet=AllocateZeroPool(sizeof(*invalidFormSet));invalidFormSet->Signature=FORM_BROWSER_FORMSET_SIGNATURE;
  InitializeListHead(&invalidFormSet->FormListHead);InitializeListHead(&invalidFormSet->StorageListHead);
  InsertTailList(&gBrowserFormSetList,&invalidFormSet->Link);expectedScope=ModernSetupReviewSystem;
  memcpy(after,before,sizeof(after));CHECK(ModernReviewSubmit(NULL,NULL,SystemLevel)==EFI_SUCCESS);CHECK(invalidFormSet==NULL);
  // A suspended outer context's password alias shares the nested scalar's store.
  BROWSER_CONTEXT outer={0};FORM_BROWSER_FORMSET outerFs={0};FORM_BROWSER_FORM outerForm={0};FORM_BROWSER_STATEMENT secret={0};
  InitializeListHead(&outer.FormSetList);InitializeListHead(&outerFs.FormListHead);InitializeListHead(&outerForm.StatementListHead);
  InsertTailList(&outer.FormSetList,&outerFs.Link);InsertTailList(&outerFs.FormListHead,&outerForm.Link);InsertTailList(&outerForm.StatementListHead,&secret.Link);
  secret.Storage=&store;secret.Operand=EFI_IFR_PASSWORD_OP;secret.StorageWidth=q[0].StorageWidth;
  gBrowserContextCount=2;CHECK(!ReviewAliasesSafe(&store,&q[0]));prev=calls;
  CHECK(ModernReviewSubmit(&fs,&form,FormSetLevel)==EFI_ABORTED&&calls==prev);gBrowserContextCount=1;
#undef ModernReviewSubmit
  // Retained approval rejects late edit/request/serialized-payload mutations.
  after[0]++;expectedCount=1;expectedScope=ModernSetupReviewFormSet;
  CHECK(ModernReviewSubmit(&fs,&form,FormSetLevel)==EFI_SUCCESS);
  CHAR16 *payload;CHECK(StorageToConfigResp(&store,&payload,ss.ConfigRequest,TRUE)==EFI_SUCCESS);
  CHECK(ModernReviewRouteGuard(&store,ss.ConfigRequest,payload)==EFI_SUCCESS);FreePool(payload);
  after[0]++;CHECK(StorageToConfigResp(&store,&payload,ss.ConfigRequest,TRUE)==EFI_SUCCESS);
  CHECK(ModernReviewRouteGuard(&store,ss.ConfigRequest,payload)==EFI_ABORTED);FreePool(payload);after[0]--;
  CHECK(StorageToConfigResp(&store,&payload,ss.ConfigRequest,TRUE)==EFI_SUCCESS);
  CHECK(ModernReviewRouteGuard(&store,L"GUID=other&OFFSET=0&WIDTH=20",payload)==EFI_ABORTED);
  mReviewContextEpoch++;CHECK(ModernReviewRouteGuard(&store,ss.ConfigRequest,payload)==EFI_ABORTED);FreePool(payload);ModernReviewFinish();
  // Execute native callback-submit rejection followed by actual CHANGED flag block.
  FORM_BROWSER_STATEMENT action={0};action.QuestionFlags=EFI_IFR_FLAG_RESET_REQUIRED|EFI_IFR_FLAG_RECONNECT_REQUIRED;
  gCallbackReconnect=TRUE;CHECK(HostChanged(&sel,&action)==EFI_WARN_WRITE_FAILURE);
  CHECK(!gCallbackReconnect&&!gFlagReconnect&&!gResetRequiredFormLevel&&!gResetRequiredSystemLevel);
  // Two native routes: first driver mutates second store after user confirmation.
  BROWSER_STORAGE second=store;UINT8 b2[32]={0},a2[32]={1};second.Buffer=b2;second.EditBuffer=a2;
  FORMSET_STORAGE ss2={0};ss2.Signature=FORMSET_STORAGE_SIGNATURE;ss2.BrowserStorage=&second;ss2.ConfigRequest=ss.ConfigRequest;ss2.ElementCount=1;
  InsertTailList(&fs.StorageListHead,&ss2.Link);InitializeListHead(&fs.SaveFailStorageListHead);InitializeListHead(&gBrowserSaveFailFormSetList);
  otherStore=&second;routing.RouteConfig=route;routeMode=1;routeCalls=0;expectedCount=2;
  CHECK(ModernReviewSubmit(&fs,&form,FormSetLevel)==EFI_SUCCESS);
  CHECK(SubmitForFormSet(&fs,FALSE)==EFI_ABORTED);CHECK(routeCalls==1&&submitted==0&&a2[0]==2&&b2[0]==0);ModernReviewFinish();
  // First route failure followed by serialization failure drains the failure list.
  routeMode=2;routeCalls=0;serializeFailStore=NULL;expectedCount=1;
  CHECK(ModernReviewSubmit(&fs,&form,FormSetLevel)==EFI_SUCCESS);
  CHECK(SubmitForFormSet(&fs,FALSE)==EFI_OUT_OF_RESOURCES);CHECK(routeCalls==1&&IsListEmpty(&fs.SaveFailStorageListHead)&&ss.RestoreConfigRequest==NULL);ModernReviewFinish();serializeFailStore=NULL;
  // Retry uses the same intrusive failure nodes without duplicate insertion.
  routeCalls=0;CHECK(ModernReviewSubmit(&fs,&form,FormSetLevel)==EFI_SUCCESS);
  CHECK(SubmitForFormSet(&fs,FALSE)==EFI_OUT_OF_RESOURCES);CHECK(IsListEmpty(&fs.SaveFailStorageListHead));ModernReviewFinish();serializeFailStore=NULL;
  // Form-level path must also drain failures when later serialization fails.
  FORM_BROWSER_CONFIG_REQUEST req2={0};req2.Signature=FORM_BROWSER_CONFIG_REQUEST_SIGNATURE;req2.Storage=&second;req2.ElementCount=1;req2.ConfigRequest=ss2.ConfigRequest;
  req.ConfigRequest=ss.ConfigRequest;InsertTailList(&form.ConfigRequestHead,&req2.Link);
  expectedScope=ModernSetupReviewForm;routeCalls=0;CHECK(ModernReviewSubmit(&fs,&form,FormLevel)==EFI_SUCCESS);
  CHECK(SubmitForForm(&fs,&form)==EFI_OUT_OF_RESOURCES);CHECK(routeCalls==1&&IsListEmpty(&gBrowserSaveFailFormSetList)&&req.RestoreConfigRequest==NULL);ModernReviewFinish();serializeFailStore=NULL;
  // Real generated LoadStorage + native destructive conversion and setter.
  // Never manually set the name/value baseline flag.
  BROWSER_STORAGE nv={0};NAME_VALUE_NODE nodes[2]={{0}};FORMSET_STORAGE nss={0};
  nv.Type=EFI_HII_VARSTORE_NAME_VALUE;InitializeListHead(&nv.NameValueListHead);
  for(int i=0;i<2;i++){nodes[i].Signature=NAME_VALUE_NODE_SIGNATURE;nodes[i].Name=i?L"nameLong":L"name";InsertTailList(&nv.NameValueListHead,&nodes[i].Link);}
  nss.Signature=FORMSET_STORAGE_SIGNATURE;nss.BrowserStorage=&nv;nss.ElementCount=2;
  nss.ConfigHdr=L"GUID=x&NAME=&PATH=00";nss.ConfigRequest=L"GUID=x&NAME=&PATH=00&name&nameLong";
  InsertTailList(&fs.StorageListHead,&nss.Link);
  RemoveEntryList(&ss2.Link);RemoveEntryList(&req2.Link);
  memcpy(after,before,sizeof(after));after[0]++;
  expectedScope=ModernSetupReviewFormSet;expectedCount=1;routeMode=0;routeCalls=0;
  extractResponse=L"GUID=x&NAME=&PATH=00&name=1234&nameLong=abcd";
  LoadStorage(&fs,&nss);CHECK(nv.ModernReviewBaselineKnown);
  CHECK(StrCmp(nodes[0].Value,L"1234")==0&&StrCmp(nodes[1].Value,L"abcd")==0);
  CHECK(ModernReviewSubmit(&fs,&form,FormSetLevel)==EFI_SUCCESS);
  CHECK(SubmitForFormSet(&fs,FALSE)==EFI_SUCCESS&&routeCalls==2);ModernReviewFinish();
  after[0]++;nodes[0].EditValue[0]='f';expectedCount=2;expectedComplete=FALSE;routeCalls=0;
  CHECK(ModernReviewSubmit(&fs,&form,FormSetLevel)==EFI_ABORTED&&routeCalls==0);
  CONST CHAR16 *badResponses[]={
    NULL, // driver success without response
    L"", // missing response content
    L"GUID=x&NAME=&PATH=00", // no names
    L"GUID=x&NAME=&PATH=00&name=1234", // missing requested name
    L"GUID=x&NAME=&PATH=00&nameLong=1234&nameLonger=abcd", // prefix collision
    L"GUID=x&NAME=&PATH=00&name=1234&name=abcd", // duplicate
    L"GUID=x&NAME=&PATH=00&name=1234&nameLong=zz", // nonhex
    L"GUID=x&NAME=&PATH=00&name=1234&nameLong=abc", // truncated byte
    L"GUID=x&NAME=&PATH=00&name=1234&nameLong=abcd&", // suffix
    L"GUID=wrong&NAME=&PATH=00&name=1234&nameLong=abcd",
    L"GUID=x&NAME=&PATH=00&name=1234&nameLong"};
  for(UINTN i=0;i<sizeof(badResponses)/sizeof(badResponses[0])+3;i++){
    FreePool(nv.ConfigRequest);nv.ConfigRequest=NULL;nv.Initialized=FALSE;
    extractResponse=i<sizeof(badResponses)/sizeof(badResponses[0])?badResponses[i]:L"GUID=x&NAME=&PATH=00&name=1234&nameLong=abcd";
    extractStatus=i==sizeof(badResponses)/sizeof(badResponses[0])?EFI_DEVICE_ERROR:EFI_SUCCESS;
    syncStatus=i==sizeof(badResponses)/sizeof(badResponses[0])+1?EFI_DEVICE_ERROR:EFI_SUCCESS;
    failSetter=i==sizeof(badResponses)/sizeof(badResponses[0])+2;
    LoadStorage(&fs,&nss);CHECK(!nv.ModernReviewBaselineKnown);failSetter=FALSE;
    CHECK(ModernReviewSubmit(&fs,&form,FormSetLevel)==EFI_ABORTED&&routeCalls==0);
  }
  // Success-returning synchronization that fails to update nodes is not trust.
  FreePool(nv.ConfigRequest);nv.ConfigRequest=NULL;nv.Initialized=FALSE;
  syncStatus=EFI_SUCCESS;extractStatus=EFI_SUCCESS;skipSync=TRUE;
  extractResponse=L"GUID=x&NAME=&PATH=00&name=ffff&nameLong=eeee";
  LoadStorage(&fs,&nss);CHECK(!nv.ModernReviewBaselineKnown);skipSync=FALSE;
  CHECK(ModernReviewSubmit(&fs,&form,FormSetLevel)==EFI_ABORTED&&routeCalls==0);
  // A request omitting a real storage node cannot bless the entire baseline.
  FreePool(nv.ConfigRequest);nv.ConfigRequest=NULL;nv.Initialized=FALSE;
  nss.ConfigRequest=L"GUID=x&NAME=&PATH=00&name";extractResponse=L"GUID=x&NAME=&PATH=00&name=1234";
  LoadStorage(&fs,&nss);CHECK(!nv.ModernReviewBaselineKnown);
  CHECK(ModernReviewSubmit(&fs,&form,FormSetLevel)==EFI_ABORTED&&routeCalls==0);
  FreePool(nv.ConfigRequest);for(int i=0;i<2;i++){FreePool(nodes[i].Value);FreePool(nodes[i].EditValue);}
  puts("PASS: native name/value LoadStorage: unchanged permits buffer route; changed, incomplete, prefix collision, malformed, extraction/sync/setter failures block");
  puts("PASS: runtime approvals, destructive validation, nested fail-closed, full extraction coverage and native cancellation");return 0;
}
