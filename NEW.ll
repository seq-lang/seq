; ModuleID = 'NEW'
source_filename = "/dev/fd/12"
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-darwin18.7.0"

%"range::__iter__.generator[int].range.Frame" = type { void (%"range::__iter__.generator[int].range.Frame"*)*, void (%"range::__iter__.generator[int].range.Frame"*)*, i64, i2, i64, i64, i64, i64, i64 }
%"SAM::__iter__.generator[SAMRecord].SAM.Frame" = type { void (%"SAM::__iter__.generator[SAMRecord].SAM.Frame"*)*, void (%"SAM::__iter__.generator[SAMRecord].SAM.Frame"*)*, i8*, i2, i8*, i8* }
%"SAM::_iter.generator[ptr[byte]].SAM.Frame" = type { void (%"SAM::_iter.generator[ptr[byte]].SAM.Frame"*)*, void (%"SAM::_iter.generator[ptr[byte]].SAM.Frame"*)*, i8*, i2, i8* }

@seq.var.113 = internal unnamed_addr global i1 false
@seq.var.114 = internal unnamed_addr global i1 false
@str_literal.8 = private constant [4 x i8] c"inf\00", align 1
@str_literal.11 = private constant [36 x i8] c"could not convert string to float: \00", align 1
@str_literal.12 = private constant [11 x i8] c"ValueError\00", align 1
@str_literal.13 = private constant [1 x i8] zeroinitializer, align 1
@str_literal.14 = private constant [1 x i8] zeroinitializer, align 1
@str_literal.15 = private constant [9 x i8] c"__init__\00", align 1
@str_literal.16 = private constant [63 x i8] c"/Users/inumanag/Desktop/Projekti/seq/cpp/stdlib/core/float.seq\00", align 1
@str_literal.17 = private constant [4 x i8] c"nan\00", align 1
@seq.var.1810 = private unnamed_addr global i8* null
@seq.var.1817 = private unnamed_addr global i8* null
@str_literal.19 = private constant [26 x i8] c"string index out of range\00", align 1
@str_literal.20 = private constant [11 x i8] c"IndexError\00", align 1
@str_literal.21 = private constant [1 x i8] zeroinitializer, align 1
@str_literal.22 = private constant [1 x i8] zeroinitializer, align 1
@str_literal.23 = private constant [12 x i8] c"__getitem__\00", align 1
@str_literal.24 = private constant [61 x i8] c"/Users/inumanag/Desktop/Projekti/seq/cpp/stdlib/core/str.seq\00", align 1
@str_literal.25 = private constant [2 x i8] c"=\00", align 1
@str_literal.26 = private constant [63 x i8] c"/Users/inumanag/Desktop/Projekti/seq/cpp/stdlib/core/slice.seq\00", align 1
@str_literal.27 = private constant [1 x i8] zeroinitializer, align 1
@str_literal.29 = private constant [9 x i8] c"KeyError\00", align 1
@str_literal.30 = private constant [1 x i8] zeroinitializer, align 1
@str_literal.31 = private constant [1 x i8] zeroinitializer, align 1
@str_literal.32 = private constant [12 x i8] c"__getitem__\00", align 1
@str_literal.33 = private constant [74 x i8] c"/Users/inumanag/Desktop/Projekti/seq/cpp/stdlib/core/collections/dict.seq\00", align 1
@str_literal.34 = private constant [11 x i8] c"SEQ_PYTHON\00", align 1
@str_literal.35 = private constant [13 x i8] c"libpython3.7\00", align 1
@str_literal.36 = private constant [2 x i8] c"/\00", align 1
@str_literal.37 = private constant [12 x i8] c"__getitem__\00", align 1
@str_literal.38 = private constant [74 x i8] c"/Users/inumanag/Desktop/Projekti/seq/cpp/stdlib/core/collections/dict.seq\00", align 1
@str_literal.39 = private constant [61 x i8] c"/Users/inumanag/Desktop/Projekti/seq/cpp/stdlib/core/str.seq\00", align 1
@str_literal.40 = private constant [7 x i8] c"CError\00", align 1
@str_literal.41 = private constant [1 x i8] zeroinitializer, align 1
@str_literal.42 = private constant [1 x i8] zeroinitializer, align 1
@str_literal.43 = private constant [7 x i8] c"dlopen\00", align 1
@str_literal.44 = private constant [64 x i8] c"/Users/inumanag/Desktop/Projekti/seq/cpp/stdlib/core/dlopen.seq\00", align 1
@str_literal.45 = private constant [7 x i8] c"_dlsym\00", align 1
@str_literal.46 = private constant [64 x i8] c"/Users/inumanag/Desktop/Projekti/seq/cpp/stdlib/core/dlopen.seq\00", align 1
@str_literal.47 = private constant [12 x i8] c"PyTuple_New\00", align 1
@str_literal.48 = private constant [12 x i8] c"PyErr_Fetch\00", align 1
@str_literal.49 = private constant [13 x i8] c"PyObject_Str\00", align 1
@str_literal.50 = private constant [26 x i8] c"PyUnicode_AsEncodedString\00", align 1
@str_literal.51 = private constant [6 x i8] c"utf-8\00", align 1
@str_literal.52 = private constant [17 x i8] c"PyBytes_AsString\00", align 1
@str_literal.53 = private constant [10 x i8] c"Py_DecRef\00", align 1
@str_literal.54 = private constant [7 x i8] c"ignore\00", align 1
@str_literal.55 = private constant [23 x i8] c"<empty Python message>\00", align 1
@str_literal.56 = private constant [23 x i8] c"PyObject_GetAttrString\00", align 1
@str_literal.57 = private constant [9 x i8] c"__name__\00", align 1
@str_literal.58 = private constant [7 x i8] c"ignore\00", align 1
@str_literal.59 = private constant [1 x i8] zeroinitializer, align 1
@str_literal.60 = private constant [8 x i8] c"PyError\00", align 1
@str_literal.61 = private constant [1 x i8] zeroinitializer, align 1
@str_literal.62 = private constant [1 x i8] zeroinitializer, align 1
@str_literal.63 = private constant [10 x i8] c"exc_check\00", align 1
@str_literal.64 = private constant [64 x i8] c"/Users/inumanag/Desktop/Projekti/seq/cpp/stdlib/core/python.seq\00", align 1
@str_literal.65 = private constant [16 x i8] c"PyTuple_SetItem\00", align 1
@str_literal.66 = private constant [16 x i8] c"PyTuple_GetItem\00", align 1
@str_literal.67 = private constant [18 x i8] c"test/data/toy.sam\00", align 1
@str_literal.68 = private constant [2 x i8] c"r\00", align 1
@str_literal.69 = private constant [6 x i8] c"file \00", align 1
@str_literal.70 = private constant [21 x i8] c" could not be opened\00", align 1
@str_literal.71 = private constant [8 x i8] c"IOError\00", align 1
@str_literal.72 = private constant [1 x i8] zeroinitializer, align 1
@str_literal.73 = private constant [1 x i8] zeroinitializer, align 1
@str_literal.74 = private constant [9 x i8] c"__init__\00", align 1
@str_literal.75 = private constant [60 x i8] c"/Users/inumanag/Desktop/Projekti/seq/cpp/stdlib/bio/bam.seq\00", align 1
@str_literal.77 = private constant [33 x i8] c"I/O operation on closed SAM file\00", align 1
@str_literal.78 = private constant [13 x i8] c"_ensure_open\00", align 1
@str_literal.79 = private constant [60 x i8] c"/Users/inumanag/Desktop/Projekti/seq/cpp/stdlib/bio/bam.seq\00", align 1
@str_literal.80 = private constant [30 x i8] c"SAM read failed with status: \00", align 1
@str_literal.81 = private constant [6 x i8] c"_iter\00", align 1
@str_literal.82 = private constant [60 x i8] c"/Users/inumanag/Desktop/Projekti/seq/cpp/stdlib/bio/bam.seq\00", align 1
@str_literal.83 = private constant [2 x i8] c"\0A\00", align 1
@"seq.typeidx.<all>" = private constant { i32 } zeroinitializer
@seq.var.1844.0 = internal unnamed_addr global i8* null
@seq.var.1847.0 = internal unnamed_addr global i64 0
@seq.var.1847.1 = internal unnamed_addr global i8* null

; Function Attrs: noinline uwtable
define void @seq.main() local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  store i1 true, i1* @seq.var.113, align 1
  store i1 true, i1* @seq.var.114, align 1
  %0 = call double @"float::__init__.float.float.str"(double undef, { i64, i8* } { i64 3, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @str_literal.8, i32 0, i32 0) })
  %1 = call double @"float::__init__.float.float.str"(double undef, { i64, i8* } { i64 3, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @str_literal.17, i32 0, i32 0) })
  %2 = call i8* @seq_alloc(i64 24)
  call void @"list[int]::__init__.void.list[int]"(i8* %2)
  call void @"list[int]::append.void.list[int].int"(i8* %2, i64 1)
  call void @"list[int]::append.void.list[int].int"(i8* %2, i64 1)
  call void @"list[int]::append.void.list[int].int"(i8* %2, i64 2)
  call void @"list[int]::append.void.list[int].int"(i8* %2, i64 6)
  call void @"list[int]::append.void.list[int].int"(i8* %2, i64 24)
  call void @"list[int]::append.void.list[int].int"(i8* %2, i64 120)
  call void @"list[int]::append.void.list[int].int"(i8* %2, i64 720)
  call void @"list[int]::append.void.list[int].int"(i8* %2, i64 5040)
  call void @"list[int]::append.void.list[int].int"(i8* %2, i64 40320)
  call void @"list[int]::append.void.list[int].int"(i8* %2, i64 362880)
  call void @"list[int]::append.void.list[int].int"(i8* %2, i64 3628800)
  call void @"list[int]::append.void.list[int].int"(i8* %2, i64 39916800)
  call void @"list[int]::append.void.list[int].int"(i8* %2, i64 479001600)
  call void @"list[int]::append.void.list[int].int"(i8* %2, i64 6227020800)
  call void @"list[int]::append.void.list[int].int"(i8* %2, i64 87178291200)
  call void @"list[int]::append.void.list[int].int"(i8* %2, i64 1307674368000)
  call void @"list[int]::append.void.list[int].int"(i8* %2, i64 20922789888000)
  call void @"list[int]::append.void.list[int].int"(i8* %2, i64 355687428096000)
  call void @"list[int]::append.void.list[int].int"(i8* %2, i64 6402373705728000)
  call void @"list[int]::append.void.list[int].int"(i8* %2, i64 121645100408832000)
  call void @"list[int]::append.void.list[int].int"(i8* %2, i64 2432902008176640000)
  %3 = call i8* @seq_alloc(i64 56)
  call void @"dict[str,ptr[byte]]::__init__.void.dict[str,ptr[byte]]"(i8* %3)
  store i8* %3, i8** @seq.var.1810, align 8
  %4 = call i8* @seq_alloc(i64 56)
  call void @"dict[str,ptr[byte]]::__init__.void.dict[str,ptr[byte]]"(i8* %4)
  store i8* %4, i8** @seq.var.1817, align 8
  %5 = call { i8* } @"EnvMap::__init__.EnvMap.EnvMap"({ i8* } undef)
  %6 = extractvalue { i8* } %5, 0
  store i8* %6, i8** @seq.var.1844.0, align 8
  %7 = call { i64, i8* } @getenv.str.str.str({ i64, i8* } { i64 10, i8* getelementptr inbounds ([11 x i8], [11 x i8]* @str_literal.34, i32 0, i32 0) }, { i64, i8* } { i64 12, i8* getelementptr inbounds ([13 x i8], [13 x i8]* @str_literal.35, i32 0, i32 0) })
  %.elt = extractvalue { i64, i8* } %7, 0
  store i64 %.elt, i64* @seq.var.1847.0, align 8
  %.elt1 = extractvalue { i64, i8* } %7, 1
  store i8* %.elt1, i8** @seq.var.1847.1, align 8
  %8 = call i8* @seq_alloc(i64 56)
  call void @"dict[str,pyobj]::__init__.void.dict[str,pyobj]"(i8* %8)
  %9 = call i8* @seq_alloc(i64 32)
  call void @"SAM::__init__.void.SAM.str"(i8* %9, { i64, i8* } { i64 17, i8* getelementptr inbounds ([18 x i8], [18 x i8]* @str_literal.67, i32 0, i32 0) })
  %10 = call i8* @"SAM::__iter__.generator[SAMRecord].SAM"(i8* %9)
  %11 = bitcast i8* %10 to void (i8*)**
  %12 = load void (i8*)*, void (i8*)** %11, align 8
  call fastcc void %12(i8* %10)
  %13 = bitcast i8* %10 to i8**
  %14 = load i8*, i8** %13, align 8
  %15 = icmp eq i8* %14, null
  br i1 %15, label %cleanup, label %body.lr.ph

body.lr.ph:                                       ; preds = %preamble
  %16 = getelementptr inbounds i8, i8* %10, i64 16
  %17 = bitcast i8* %16 to i8**
  br label %body

body:                                             ; preds = %body, %body.lr.ph
  %18 = load i8*, i8** %17, align 8
  %19 = call { i64, i8* } @"SAMRecord::name.str.SAMRecord"(i8* %18)
  %20 = call { i64, i8* } @"str::__str__.str.str"({ i64, i8* } %19)
  call void @seq_print({ i64, i8* } %20)
  %21 = call { i64, i8* } @"str::__str__.str.str"({ i64, i8* } { i64 1, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @str_literal.83, i32 0, i32 0) })
  call void @seq_print({ i64, i8* } %21)
  %22 = load void (i8*)*, void (i8*)** %11, align 8
  call fastcc void %22(i8* %10)
  %23 = load i8*, i8** %13, align 8
  %24 = icmp eq i8* %23, null
  br i1 %24, label %cleanup, label %body

cleanup:                                          ; preds = %body, %preamble
  %25 = getelementptr inbounds i8, i8* %10, i64 8
  %26 = bitcast i8* %25 to void (i8*)**
  %27 = load void (i8*)*, void (i8*)** %26, align 8
  call fastcc void %27(i8* nonnull %10)
  ret void
}

; Function Attrs: noinline uwtable
declare i32 @seq_personality(i32, i32, i64, i8*, i8*) #0

; Function Attrs: noinline uwtable
declare void @seq_init() local_unnamed_addr #0

; Function Attrs: argmemonly noinline nounwind readonly uwtable
declare i64 @strlen(i8* nocapture) local_unnamed_addr #1

; Function Attrs: noinline uwtable
define double @"float::__init__.float.float.str"(double, { i64, i8* }) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.fca.1.extract22 = extractvalue { i64, i8* } %1, 1
  %2 = alloca [32 x i8], align 1
  %3 = alloca i8*, align 8
  %.sub = getelementptr inbounds [32 x i8], [32 x i8]* %2, i64 0, i64 0
  %4 = call i64 @"len[str].int.str"({ i64, i8* } %1)
  %.fca.1.insert = insertvalue { i64, i8* } { i64 32, i8* undef }, i8* %.sub, 1
  %5 = call i64 @"len[array[byte]].int.array[byte]"({ i64, i8* } %.fca.1.insert)
  %6 = icmp sge i64 %4, %5
  br i1 %6, label %7, label %10

; <label>:7:                                      ; preds = %preamble
  %8 = add i64 %4, 1
  %9 = call i8* @"alloc_atomic.ptr[byte].int"(i64 %8)
  br label %10

; <label>:10:                                     ; preds = %7, %preamble
  %11 = phi i8* [ %9, %7 ], [ %.sub, %preamble ]
  call void @seq.memcpy(i8* %11, i8* %.fca.1.extract22, i64 %4)
  %12 = getelementptr i8, i8* %11, i64 %4
  store i8 0, i8* %12, align 1
  store i8* null, i8** %3, align 8
  %13 = call double @strtod(i8* %11, i8** nonnull %3)
  br i1 %6, label %14, label %15

; <label>:14:                                     ; preds = %10
  call void @"free.void.ptr[byte]"(i8* %11)
  br label %15

; <label>:15:                                     ; preds = %14, %10
  %16 = load i8*, i8** %3, align 8
  %17 = icmp eq i8* %16, %12
  br i1 %17, label %28, label %18

; <label>:18:                                     ; preds = %15
  %19 = call { i64, i8* } @"str::__add__.str.str.str"({ i64, i8* } { i64 35, i8* getelementptr inbounds ([36 x i8], [36 x i8]* @str_literal.11, i32 0, i32 0) }, { i64, i8* } %1)
  %20 = call i8* @seq_alloc(i64 80)
  call void @"ValueError::__init__.void.ValueError.str"(i8* %20, { i64, i8* } %19)
  %.repack60.repack = getelementptr inbounds i8, i8* %20, i64 32
  %21 = bitcast i8* %.repack60.repack to i64*
  store i64 8, i64* %21, align 8
  %.repack60.repack70 = getelementptr inbounds i8, i8* %20, i64 40
  %22 = bitcast i8* %.repack60.repack70 to i8**
  store i8* getelementptr inbounds ([9 x i8], [9 x i8]* @str_literal.15, i64 0, i64 0), i8** %22, align 8
  %.repack62.repack = getelementptr inbounds i8, i8* %20, i64 48
  %23 = bitcast i8* %.repack62.repack to i64*
  store i64 62, i64* %23, align 8
  %.repack62.repack68 = getelementptr inbounds i8, i8* %20, i64 56
  %24 = bitcast i8* %.repack62.repack68 to i8**
  store i8* getelementptr inbounds ([63 x i8], [63 x i8]* @str_literal.16, i64 0, i64 0), i8** %24, align 8
  %.repack64 = getelementptr inbounds i8, i8* %20, i64 64
  %25 = bitcast i8* %.repack64 to i64*
  store i64 21, i64* %25, align 8
  %.repack66 = getelementptr inbounds i8, i8* %20, i64 72
  %26 = bitcast i8* %.repack66 to i64*
  store i64 13, i64* %26, align 8
  %27 = call i8* @seq_alloc_exc(i32 -302408385, i8* %20)
  call void @seq_throw(i8* %27)
  unreachable

; <label>:28:                                     ; preds = %15
  ret double %13
}

; Function Attrs: noinline norecurse nounwind readnone uwtable
define i64 @"len[str].int.str"({ i64, i8* }) local_unnamed_addr #2 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %1 = call i64 @"str::__len__.int.str"({ i64, i8* } %0)
  ret i64 %1
}

; Function Attrs: noinline norecurse nounwind readnone uwtable
define i64 @"str::__len__.int.str"({ i64, i8* }) local_unnamed_addr #2 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.fca.0.extract = extractvalue { i64, i8* } %0, 0
  ret i64 %.fca.0.extract
}

; Function Attrs: noinline norecurse nounwind readnone uwtable
define i64 @"len[array[byte]].int.array[byte]"({ i64, i8* }) local_unnamed_addr #2 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %1 = call i64 @seq.magic.__len__.1({ i64, i8* } %0)
  ret i64 %1
}

; Function Attrs: noinline norecurse nounwind readnone uwtable
define i64 @seq.magic.__len__.1({ i64, i8* }) local_unnamed_addr #2 {
entry:
  %1 = extractvalue { i64, i8* } %0, 0
  ret i64 %1
}

; Function Attrs: noinline nounwind uwtable
define noalias i8* @"alloc_atomic.ptr[byte].int"(i64) local_unnamed_addr #3 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %1 = call i8* @seq_alloc_atomic(i64 %0)
  ret i8* %1
}

; Function Attrs: inaccessiblememonly noinline nounwind uwtable
declare noalias i8* @seq_alloc_atomic(i64) local_unnamed_addr #4

; Function Attrs: noinline nounwind uwtable
define void @seq.memcpy(i8* nocapture, i8* nocapture readonly, i64) local_unnamed_addr #3 {
entry:
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %0, i8* %1, i64 %2, i32 1, i1 false)
  ret void
}

; Function Attrs: argmemonly nounwind
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* nocapture writeonly, i8* nocapture readonly, i64, i32, i1) #5

; Function Attrs: noinline nounwind uwtable
declare double @strtod(i8* readonly, i8** nocapture) local_unnamed_addr #3

; Function Attrs: noinline uwtable
define void @"free.void.ptr[byte]"(i8*) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  call void @seq_free(i8* %0)
  ret void
}

; Function Attrs: noinline uwtable
declare void @seq_free(i8*) local_unnamed_addr #0

; Function Attrs: noinline nounwind uwtable
declare i8* @seq_alloc_exc(i32, i8*) local_unnamed_addr #3

; Function Attrs: noinline noreturn uwtable
declare void @seq_throw(i8*) local_unnamed_addr #6

; Function Attrs: noinline nounwind uwtable
define { i64, i8* } @"str::__add__.str.str.str"({ i64, i8* }, { i64, i8* }) local_unnamed_addr #3 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.fca.0.extract18 = extractvalue { i64, i8* } %0, 0
  %.fca.1.extract19 = extractvalue { i64, i8* } %0, 1
  %.fca.0.extract = extractvalue { i64, i8* } %1, 0
  %.fca.1.extract = extractvalue { i64, i8* } %1, 1
  %2 = add i64 %.fca.0.extract, %.fca.0.extract18
  %3 = call i8* @seq_alloc_atomic(i64 %2)
  call void @seq.memcpy(i8* %3, i8* %.fca.1.extract19, i64 %.fca.0.extract18)
  %4 = getelementptr i8, i8* %3, i64 %.fca.0.extract18
  call void @seq.memcpy(i8* %4, i8* %.fca.1.extract, i64 %.fca.0.extract)
  %5 = insertvalue { i64, i8* } undef, i8* %3, 1
  %6 = insertvalue { i64, i8* } %5, i64 %2, 0
  ret { i64, i8* } %6
}

; Function Attrs: inaccessiblememonly noinline nounwind uwtable
declare noalias i8* @seq_alloc(i64) local_unnamed_addr #4

; Function Attrs: noinline norecurse nounwind uwtable
define void @"ValueError::__init__.void.ValueError.str"(i8* nocapture, { i64, i8* }) local_unnamed_addr #7 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.repack.repack = bitcast i8* %0 to i64*
  store i64 10, i64* %.repack.repack, align 8
  %.repack.repack19 = getelementptr inbounds i8, i8* %0, i64 8
  %2 = bitcast i8* %.repack.repack19 to i8**
  store i8* getelementptr inbounds ([11 x i8], [11 x i8]* @str_literal.12, i64 0, i64 0), i8** %2, align 8
  %.repack3.repack = getelementptr inbounds i8, i8* %0, i64 16
  %3 = bitcast i8* %.repack3.repack to i64*
  %.elt4.elt = extractvalue { i64, i8* } %1, 0
  store i64 %.elt4.elt, i64* %3, align 8
  %.repack3.repack17 = getelementptr inbounds i8, i8* %0, i64 24
  %4 = bitcast i8* %.repack3.repack17 to i8**
  %.elt4.elt18 = extractvalue { i64, i8* } %1, 1
  store i8* %.elt4.elt18, i8** %4, align 8
  %.repack5.repack = getelementptr inbounds i8, i8* %0, i64 32
  %5 = bitcast i8* %.repack5.repack to i64*
  store i64 0, i64* %5, align 8
  %.repack5.repack15 = getelementptr inbounds i8, i8* %0, i64 40
  %6 = bitcast i8* %.repack5.repack15 to i8**
  store i8* getelementptr inbounds ([1 x i8], [1 x i8]* @str_literal.13, i64 0, i64 0), i8** %6, align 8
  %.repack7.repack = getelementptr inbounds i8, i8* %0, i64 48
  %7 = bitcast i8* %.repack7.repack to i64*
  store i64 0, i64* %7, align 8
  %.repack7.repack13 = getelementptr inbounds i8, i8* %0, i64 56
  %8 = bitcast i8* %.repack7.repack13 to i8**
  store i8* getelementptr inbounds ([1 x i8], [1 x i8]* @str_literal.14, i64 0, i64 0), i8** %8, align 8
  %.repack9 = getelementptr inbounds i8, i8* %0, i64 64
  call void @llvm.memset.p0i8.i64(i8* nonnull %.repack9, i8 0, i64 16, i32 8, i1 false)
  ret void
}

; Function Attrs: noinline nounwind uwtable
define void @"list[int]::__init__.void.list[int]"(i8* nocapture) local_unnamed_addr #3 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %1 = call i8* @seq_alloc_atomic(i64 80)
  %.repack.repack = bitcast i8* %0 to i64*
  store i64 10, i64* %.repack.repack, align 8
  %.repack.repack6 = getelementptr inbounds i8, i8* %0, i64 8
  %2 = bitcast i8* %.repack.repack6 to i8**
  store i8* %1, i8** %2, align 8
  %.repack4 = getelementptr inbounds i8, i8* %0, i64 16
  %3 = bitcast i8* %.repack4 to i64*
  store i64 0, i64* %3, align 8
  ret void
}

; Function Attrs: noinline uwtable
define void @"list[int]::append.void.list[int].int"(i8* nocapture, i64) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  call void @"list[int]::_resize_if_full.void.list[int]"(i8* %0)
  %.unpack.elt7 = getelementptr inbounds i8, i8* %0, i64 8
  %2 = bitcast i8* %.unpack.elt7 to i64**
  %.unpack.unpack8 = load i64*, i64** %2, align 8
  %.elt5 = getelementptr inbounds i8, i8* %0, i64 16
  %3 = bitcast i8* %.elt5 to i64*
  %.unpack6 = load i64, i64* %3, align 8
  %4 = getelementptr i64, i64* %.unpack.unpack8, i64 %.unpack6
  store i64 %1, i64* %4, align 8
  %.unpack11 = load i64, i64* %3, align 8
  %5 = add i64 %.unpack11, 1
  store i64 %5, i64* %3, align 8
  ret void
}

; Function Attrs: noinline uwtable
define void @"list[int]::_resize_if_full.void.list[int]"(i8* nocapture) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.unpack.elt = bitcast i8* %0 to i64*
  %.unpack.unpack = load i64, i64* %.unpack.elt, align 8
  %.elt4 = getelementptr inbounds i8, i8* %0, i64 16
  %1 = bitcast i8* %.elt4 to i64*
  %.unpack5 = load i64, i64* %1, align 8
  %2 = icmp eq i64 %.unpack5, %.unpack.unpack
  br i1 %2, label %3, label %7

; <label>:3:                                      ; preds = %preamble
  %4 = mul i64 %.unpack.unpack, 3
  %5 = add i64 %4, 1
  %6 = sdiv i64 %5, 2
  call void @"list[int]::_resize.void.list[int].int"(i8* nonnull %0, i64 %6)
  br label %7

; <label>:7:                                      ; preds = %3, %preamble
  ret void
}

; Function Attrs: noinline uwtable
define void @"list[int]::_resize.void.list[int].int"(i8* nocapture, i64) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.unpack.elt6 = getelementptr inbounds i8, i8* %0, i64 8
  %2 = bitcast i8* %.unpack.elt6 to i8**
  %.unpack.unpack79 = load i8*, i8** %2, align 8
  %3 = shl i64 %1, 3
  %4 = call i8* @"realloc.ptr[byte].ptr[byte].int"(i8* %.unpack.unpack79, i64 %3)
  %.repack.repack = bitcast i8* %0 to i64*
  store i64 %1, i64* %.repack.repack, align 8
  store i8* %4, i8** %2, align 8
  ret void
}

; Function Attrs: noinline uwtable
define i8* @"realloc.ptr[byte].ptr[byte].int"(i8*, i64) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %2 = call i8* @seq_realloc(i8* %0, i64 %1)
  ret i8* %2
}

; Function Attrs: noinline uwtable
declare i8* @seq_realloc(i8*, i64) local_unnamed_addr #0

; Function Attrs: noinline norecurse nounwind readnone uwtable
define i64 @"sizeof[int].int"() local_unnamed_addr #2 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  ret i64 8
}

; Function Attrs: noinline norecurse nounwind readnone uwtable
define i64 @seq.magic.__elemsize__.2() local_unnamed_addr #2 {
entry:
  ret i64 8
}

; Function Attrs: noinline norecurse nounwind uwtable
define void @"dict[str,ptr[byte]]::__init__.void.dict[str,ptr[byte]]"(i8* nocapture) local_unnamed_addr #7 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  call void @"dict[str,ptr[byte]]::_init.void.dict[str,ptr[byte]]"(i8* %0)
  ret void
}

; Function Attrs: noinline norecurse nounwind uwtable
define void @"dict[str,ptr[byte]]::_init.void.dict[str,ptr[byte]]"(i8* nocapture) local_unnamed_addr #7 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  call void @llvm.memset.p0i8.i64(i8* %0, i8 0, i64 56, i32 8, i1 false)
  ret void
}

; Function Attrs: noinline nounwind uwtable
define { i8* } @"EnvMap::__init__.EnvMap.EnvMap"({ i8* }) local_unnamed_addr #3 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %1 = call i8* @seq_alloc(i64 56)
  call void @"dict[str,str]::__init__.void.dict[str,str]"(i8* %1)
  %2 = insertvalue { i8* } undef, i8* %1, 0
  ret { i8* } %2
}

; Function Attrs: noinline norecurse nounwind uwtable
define void @"dict[str,str]::__init__.void.dict[str,str]"(i8* nocapture) local_unnamed_addr #7 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  call void @"dict[str,str]::_init.void.dict[str,str]"(i8* %0)
  ret void
}

; Function Attrs: noinline norecurse nounwind uwtable
define void @"dict[str,str]::_init.void.dict[str,str]"(i8* nocapture) local_unnamed_addr #7 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  call void @llvm.memset.p0i8.i64(i8* %0, i8 0, i64 56, i32 8, i1 false)
  ret void
}

; Function Attrs: noinline uwtable
define { i64, i8* } @getenv.str.str.str({ i64, i8* }, { i64, i8* }) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.unpack = load i8*, i8** @seq.var.1844.0, align 8
  %2 = insertvalue { i8* } undef, i8* %.unpack, 0
  %3 = call i8 @"EnvMap::__contains__.bool.EnvMap.str"({ i8* } %2, { i64, i8* } %0)
  %4 = and i8 %3, 1
  %5 = icmp eq i8 %4, 0
  br i1 %5, label %9, label %6

; <label>:6:                                      ; preds = %preamble
  %.unpack17 = load i8*, i8** @seq.var.1844.0, align 8
  %7 = insertvalue { i8* } undef, i8* %.unpack17, 0
  %8 = call { i64, i8* } @"EnvMap::__getitem__.str.EnvMap.str"({ i8* } %7, { i64, i8* } %0)
  br label %9

; <label>:9:                                      ; preds = %6, %preamble
  %10 = phi { i64, i8* } [ %8, %6 ], [ %1, %preamble ]
  ret { i64, i8* } %10
}

; Function Attrs: noinline uwtable
define i8 @"EnvMap::__contains__.bool.EnvMap.str"({ i8* }, { i64, i8* }) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.fca.0.extract3 = extractvalue { i8* } %0, 0
  call void @"EnvMap::_init_if_needed.void.EnvMap"({ i8* } %0)
  %2 = call i8 @"dict[str,str]::__contains__.bool.dict[str,str].str"(i8* %.fca.0.extract3, { i64, i8* } %1)
  ret i8 %2
}

; Function Attrs: noinline uwtable
define void @"EnvMap::_init_if_needed.void.EnvMap"({ i8* }) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.fca.0.extract60 = extractvalue { i8* } %0, 0
  %1 = call i64 @"len[dict[str,str]].int.dict[str,str]"(i8* %.fca.0.extract60)
  %2 = icmp eq i64 %1, 0
  br i1 %2, label %3, label %exit3

; <label>:3:                                      ; preds = %preamble
  %4 = call i8** @seq_env()
  %.074 = load i8*, i8** %4, align 8
  %5 = icmp eq i8* %.074, null
  br i1 %5, label %exit3, label %body.preheader

body.preheader:                                   ; preds = %3
  br label %body

body:                                             ; preds = %while, %body.preheader
  %.076 = phi i8* [ %.0, %while ], [ %.074, %body.preheader ]
  %.06775 = phi i64 [ %27, %while ], [ 0, %body.preheader ]
  %6 = call { i64, i8* } @"str::from_ptr.str.ptr[byte]"(i8* nonnull %.076)
  %7 = call i8 @"str::__bool__.bool.str"({ i64, i8* } %6)
  %8 = and i8 %7, 1
  %9 = icmp eq i8 %8, 0
  br i1 %9, label %while, label %10

; <label>:10:                                     ; preds = %body
  %11 = call i64 @"len[str].int.str"({ i64, i8* } %6)
  %12 = icmp sgt i64 %11, 0
  br i1 %12, label %body2.preheader, label %exit

body2.preheader:                                  ; preds = %10
  br label %body2

body2:                                            ; preds = %while1, %body2.preheader
  %.06873 = phi i64 [ %17, %while1 ], [ 0, %body2.preheader ]
  %13 = call { i64, i8* } @"str::__getitem__.str.str.int"({ i64, i8* } %6, i64 %.06873)
  %14 = call i8 @"str::__eq__.bool.str.str"({ i64, i8* } %13, { i64, i8* } { i64 1, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @str_literal.25, i32 0, i32 0) })
  %15 = and i8 %14, 1
  %16 = icmp eq i8 %15, 0
  br i1 %16, label %while1, label %19

while1:                                           ; preds = %body2
  %17 = add nuw nsw i64 %.06873, 1
  %18 = icmp slt i64 %17, %11
  br i1 %18, label %body2, label %exit

; <label>:19:                                     ; preds = %body2
  %20 = insertvalue { i64, i64 } zeroinitializer, i64 %.06873, 1
  %21 = call { i64, i8* } @"str::__getitem__.str.str.slice"({ i64, i8* } %6, { i64, i64 } %20)
  %22 = add nuw i64 %.06873, 1
  %23 = insertvalue { i64 } zeroinitializer, i64 %22, 0
  %24 = call { i64, i8* } @"str::__getitem__.str.str.rslice"({ i64, i8* } %6, { i64 } %23)
  br label %exit

exit:                                             ; preds = %19, %while1, %10
  %25 = phi { i64, i8* } [ %21, %19 ], [ %6, %10 ], [ %6, %while1 ]
  %26 = phi { i64, i8* } [ %24, %19 ], [ { i64 0, i8* getelementptr inbounds ([1 x i8], [1 x i8]* @str_literal.27, i32 0, i32 0) }, %10 ], [ { i64 0, i8* getelementptr inbounds ([1 x i8], [1 x i8]* @str_literal.27, i32 0, i32 0) }, %while1 ]
  call void @"dict[str,str]::__setitem__.void.dict[str,str].str.str"(i8* %.fca.0.extract60, { i64, i8* } %25, { i64, i8* } %26)
  br label %while

while:                                            ; preds = %exit, %body
  %27 = add i64 %.06775, 1
  %28 = getelementptr i8*, i8** %4, i64 %27
  %.0 = load i8*, i8** %28, align 8
  %29 = icmp eq i8* %.0, null
  br i1 %29, label %exit3, label %body

exit3:                                            ; preds = %while, %3, %preamble
  ret void
}

; Function Attrs: noinline norecurse nounwind readonly uwtable
define i64 @"len[dict[str,str]].int.dict[str,str]"(i8* nocapture readonly) local_unnamed_addr #8 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %1 = call i64 @"dict[str,str]::__len__.int.dict[str,str]"(i8* %0)
  ret i64 %1
}

; Function Attrs: noinline norecurse nounwind readonly uwtable
define i64 @"dict[str,str]::__len__.int.dict[str,str]"(i8* nocapture readonly) local_unnamed_addr #8 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.elt1 = getelementptr inbounds i8, i8* %0, i64 8
  %1 = bitcast i8* %.elt1 to i64*
  %.unpack2 = load i64, i64* %1, align 8
  ret i64 %.unpack2
}

; Function Attrs: noinline uwtable
declare i8** @seq_env() local_unnamed_addr #0

; Function Attrs: noinline uwtable
define { i64, i8* } @"str::from_ptr.str.ptr[byte]"(i8*) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %1 = call { i64, i8* } @seq_strdup(i8* %0)
  ret { i64, i8* } %1
}

; Function Attrs: noinline uwtable
declare { i64, i8* } @seq_strdup(i8*) local_unnamed_addr #0

; Function Attrs: noinline norecurse nounwind readnone uwtable
define i8 @"str::__bool__.bool.str"({ i64, i8* }) local_unnamed_addr #2 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.fca.0.extract = extractvalue { i64, i8* } %0, 0
  %1 = icmp ne i64 %.fca.0.extract, 0
  %2 = zext i1 %1 to i8
  ret i8 %2
}

; Function Attrs: noinline uwtable
define { i64, i8* } @"str::__getitem__.str.str.int"({ i64, i8* }, i64) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.fca.1.extract = extractvalue { i64, i8* } %0, 1
  %2 = icmp slt i64 %1, 0
  %3 = call i64 @"len[str].int.str"({ i64, i8* } %0)
  br i1 %2, label %4, label %.thread

; <label>:4:                                      ; preds = %preamble
  %5 = add i64 %3, %1
  %6 = icmp sgt i64 %5, -1
  br i1 %6, label %.thread, label %.critedge

.thread:                                          ; preds = %4, %preamble
  %.057 = phi i64 [ %5, %4 ], [ %1, %preamble ]
  %7 = icmp slt i64 %.057, %3
  br i1 %7, label %16, label %.critedge

.critedge:                                        ; preds = %.thread, %4
  %8 = call i8* @seq_alloc(i64 80)
  call void @"IndexError::__init__.void.IndexError.str"(i8* %8, { i64, i8* } { i64 25, i8* getelementptr inbounds ([26 x i8], [26 x i8]* @str_literal.19, i32 0, i32 0) })
  %.repack39.repack = getelementptr inbounds i8, i8* %8, i64 32
  %9 = bitcast i8* %.repack39.repack to i64*
  store i64 11, i64* %9, align 8
  %.repack39.repack49 = getelementptr inbounds i8, i8* %8, i64 40
  %10 = bitcast i8* %.repack39.repack49 to i8**
  store i8* getelementptr inbounds ([12 x i8], [12 x i8]* @str_literal.23, i64 0, i64 0), i8** %10, align 8
  %.repack41.repack = getelementptr inbounds i8, i8* %8, i64 48
  %11 = bitcast i8* %.repack41.repack to i64*
  store i64 60, i64* %11, align 8
  %.repack41.repack47 = getelementptr inbounds i8, i8* %8, i64 56
  %12 = bitcast i8* %.repack41.repack47 to i8**
  store i8* getelementptr inbounds ([61 x i8], [61 x i8]* @str_literal.24, i64 0, i64 0), i8** %12, align 8
  %.repack43 = getelementptr inbounds i8, i8* %8, i64 64
  %13 = bitcast i8* %.repack43 to i64*
  store i64 67, i64* %13, align 8
  %.repack45 = getelementptr inbounds i8, i8* %8, i64 72
  %14 = bitcast i8* %.repack45 to i64*
  store i64 13, i64* %14, align 8
  %15 = call i8* @seq_alloc_exc(i32 1319121437, i8* %8)
  call void @seq_throw(i8* %15)
  unreachable

; <label>:16:                                     ; preds = %.thread
  %17 = getelementptr i8, i8* %.fca.1.extract, i64 %.057
  %18 = insertvalue { i64, i8* } undef, i8* %17, 1
  %19 = insertvalue { i64, i8* } %18, i64 1, 0
  ret { i64, i8* } %19
}

; Function Attrs: noinline norecurse nounwind uwtable
define void @"IndexError::__init__.void.IndexError.str"(i8* nocapture, { i64, i8* }) local_unnamed_addr #7 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.repack.repack = bitcast i8* %0 to i64*
  store i64 10, i64* %.repack.repack, align 8
  %.repack.repack19 = getelementptr inbounds i8, i8* %0, i64 8
  %2 = bitcast i8* %.repack.repack19 to i8**
  store i8* getelementptr inbounds ([11 x i8], [11 x i8]* @str_literal.20, i64 0, i64 0), i8** %2, align 8
  %.repack3.repack = getelementptr inbounds i8, i8* %0, i64 16
  %3 = bitcast i8* %.repack3.repack to i64*
  %.elt4.elt = extractvalue { i64, i8* } %1, 0
  store i64 %.elt4.elt, i64* %3, align 8
  %.repack3.repack17 = getelementptr inbounds i8, i8* %0, i64 24
  %4 = bitcast i8* %.repack3.repack17 to i8**
  %.elt4.elt18 = extractvalue { i64, i8* } %1, 1
  store i8* %.elt4.elt18, i8** %4, align 8
  %.repack5.repack = getelementptr inbounds i8, i8* %0, i64 32
  %5 = bitcast i8* %.repack5.repack to i64*
  store i64 0, i64* %5, align 8
  %.repack5.repack15 = getelementptr inbounds i8, i8* %0, i64 40
  %6 = bitcast i8* %.repack5.repack15 to i8**
  store i8* getelementptr inbounds ([1 x i8], [1 x i8]* @str_literal.21, i64 0, i64 0), i8** %6, align 8
  %.repack7.repack = getelementptr inbounds i8, i8* %0, i64 48
  %7 = bitcast i8* %.repack7.repack to i64*
  store i64 0, i64* %7, align 8
  %.repack7.repack13 = getelementptr inbounds i8, i8* %0, i64 56
  %8 = bitcast i8* %.repack7.repack13 to i8**
  store i8* getelementptr inbounds ([1 x i8], [1 x i8]* @str_literal.22, i64 0, i64 0), i8** %8, align 8
  %.repack9 = getelementptr inbounds i8, i8* %0, i64 64
  call void @llvm.memset.p0i8.i64(i8* nonnull %.repack9, i8 0, i64 16, i32 8, i1 false)
  ret void
}

; Function Attrs: noinline norecurse nounwind readonly uwtable
define i8 @"str::__eq__.bool.str.str"({ i64, i8* }, { i64, i8* }) local_unnamed_addr #8 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.fca.1.extract14 = extractvalue { i64, i8* } %0, 1
  %.fca.1.extract = extractvalue { i64, i8* } %1, 1
  %2 = call i64 @"len[str].int.str"({ i64, i8* } %0)
  %3 = call i64 @"len[str].int.str"({ i64, i8* } %1)
  %4 = icmp eq i64 %2, %3
  br i1 %4, label %5, label %exit

exit:                                             ; preds = %body, %while, %5, %preamble
  %merge = phi i8 [ 0, %preamble ], [ 1, %5 ], [ 0, %body ], [ 1, %while ]
  ret i8 %merge

; <label>:5:                                      ; preds = %preamble
  %6 = icmp sgt i64 %2, 0
  br i1 %6, label %body.preheader, label %exit

body.preheader:                                   ; preds = %5
  br label %body

while:                                            ; preds = %body
  %7 = icmp slt i64 %13, %2
  br i1 %7, label %body, label %exit

body:                                             ; preds = %while, %body.preheader
  %.034 = phi i64 [ %13, %while ], [ 0, %body.preheader ]
  %8 = getelementptr i8, i8* %.fca.1.extract14, i64 %.034
  %9 = load i8, i8* %8, align 1
  %10 = getelementptr i8, i8* %.fca.1.extract, i64 %.034
  %11 = load i8, i8* %10, align 1
  %12 = icmp eq i8 %9, %11
  %13 = add nuw nsw i64 %.034, 1
  br i1 %12, label %while, label %exit
}

; Function Attrs: noinline nounwind uwtable
define { i64, i8* } @"str::__getitem__.str.str.slice"({ i64, i8* }, { i64, i64 }) local_unnamed_addr #3 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.fca.1.extract70 = extractvalue { i64, i8* } %0, 1
  %.fca.0.extract54 = extractvalue { i64, i64 } %1, 0
  %.fca.1.extract55 = extractvalue { i64, i64 } %1, 1
  %2 = call i64 @"len[str].int.str"({ i64, i8* } %0)
  %3 = insertvalue { i1, i64 } { i1 true, i64 undef }, i64 %.fca.0.extract54, 1
  %4 = insertvalue { i1, i64 } { i1 true, i64 undef }, i64 %.fca.1.extract55, 1
  %5 = call { i64, i64, i64, i64 } @"slice::adjust_indices.tuple[int,int,int,int].int.optional[int].optional[int].optional[int]"(i64 %2, { i1, i64 } %3, { i1, i64 } %4, { i1, i64 } { i1 false, i64 undef })
  %.fca.0.extract = extractvalue { i64, i64, i64, i64 } %5, 0
  %.fca.3.extract = extractvalue { i64, i64, i64, i64 } %5, 3
  %6 = getelementptr i8, i8* %.fca.1.extract70, i64 %.fca.0.extract
  %7 = insertvalue { i64, i8* } undef, i8* %6, 1
  %8 = insertvalue { i64, i8* } %7, i64 %.fca.3.extract, 0
  ret { i64, i8* } %8
}

; Function Attrs: noinline nounwind uwtable
define { i64, i64, i64, i64 } @"slice::adjust_indices.tuple[int,int,int,int].int.optional[int].optional[int].optional[int]"(i64, { i1, i64 }, { i1, i64 }, { i1, i64 }) local_unnamed_addr #3 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.fca.0.extract42 = extractvalue { i1, i64 } %1, 0
  %.fca.1.extract43 = extractvalue { i1, i64 } %1, 1
  %.fca.0.extract = extractvalue { i1, i64 } %3, 0
  %.fca.1.extract = extractvalue { i1, i64 } %3, 1
  %4 = select i1 %.fca.0.extract, i64 %.fca.1.extract, i64 1
  %5 = icmp eq i64 %4, 0
  br i1 %5, label %assert_fail, label %assert_pass

assert_fail:                                      ; preds = %preamble
  call void @seq_assert_failed({ i64, i8* } { i64 62, i8* getelementptr inbounds ([63 x i8], [63 x i8]* @str_literal.26, i32 0, i32 0) }, i64 40)
  unreachable

assert_pass:                                      ; preds = %preamble
  %.fca.1.extract15 = extractvalue { i1, i64 } %2, 1
  %.fca.0.extract14 = extractvalue { i1, i64 } %2, 0
  %6 = icmp sgt i64 %4, 0
  %7 = add i64 %0, -1
  %8 = select i1 %.fca.0.extract42, i64 %.fca.1.extract43, i64 %7
  %9 = select i1 %.fca.0.extract42, i64 %.fca.1.extract43, i64 0
  %not. = xor i1 %6, true
  %10 = sext i1 %not. to i64
  %.sink = xor i64 %10, %0
  %.061 = select i1 %6, i64 %9, i64 %8
  %11 = select i1 %.fca.0.extract14, i64 %.fca.1.extract15, i64 %.sink
  %12 = call { i64, i64, i64, i64 } @"slice::adjust_indices.tuple[int,int,int,int].int.optional[int].optional[int].optional[int]::adjust_indices_helper.tuple[int,int,int,int].int.int.int.int"(i64 %0, i64 %.061, i64 %11, i64 %4)
  ret { i64, i64, i64, i64 } %12
}

; Function Attrs: noinline noreturn nounwind uwtable
declare void @seq_assert_failed({ i64, i8* }, i64) local_unnamed_addr #9

; Function Attrs: noinline norecurse nounwind readnone uwtable
define { i64, i64, i64, i64 } @"slice::adjust_indices.tuple[int,int,int,int].int.optional[int].optional[int].optional[int]::adjust_indices_helper.tuple[int,int,int,int].int.int.int.int"(i64, i64, i64, i64) local_unnamed_addr #2 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %4 = icmp slt i64 %1, 0
  br i1 %4, label %5, label %8

; <label>:5:                                      ; preds = %preamble
  %6 = add i64 %1, %0
  %7 = icmp slt i64 %6, 0
  %.lobit38 = ashr i64 %3, 63
  %spec.select = select i1 %7, i64 %.lobit38, i64 %6
  br label %12

; <label>:8:                                      ; preds = %preamble
  %9 = icmp slt i64 %1, %0
  br i1 %9, label %12, label %10

; <label>:10:                                     ; preds = %8
  %.lobit37 = ashr i64 %3, 63
  %11 = add i64 %.lobit37, %0
  br label %12

; <label>:12:                                     ; preds = %10, %8, %5
  %.0 = phi i64 [ %11, %10 ], [ %1, %8 ], [ %spec.select, %5 ]
  %13 = icmp slt i64 %2, 0
  br i1 %13, label %14, label %17

; <label>:14:                                     ; preds = %12
  %15 = add i64 %2, %0
  %16 = icmp slt i64 %15, 0
  %.lobit36 = ashr i64 %3, 63
  %spec.select39 = select i1 %16, i64 %.lobit36, i64 %15
  br label %21

; <label>:17:                                     ; preds = %12
  %18 = icmp slt i64 %2, %0
  br i1 %18, label %21, label %19

; <label>:19:                                     ; preds = %17
  %.lobit = ashr i64 %3, 63
  %20 = add i64 %.lobit, %0
  br label %21

; <label>:21:                                     ; preds = %19, %17, %14
  %.035 = phi i64 [ %20, %19 ], [ %2, %17 ], [ %spec.select39, %14 ]
  %22 = icmp slt i64 %3, 0
  br i1 %22, label %23, label %35

; <label>:23:                                     ; preds = %21
  %24 = icmp slt i64 %.035, %.0
  br i1 %24, label %25, label %46

; <label>:25:                                     ; preds = %23
  %26 = insertvalue { i64, i64, i64, i64 } undef, i64 %.0, 0
  %27 = insertvalue { i64, i64, i64, i64 } %26, i64 %.035, 1
  %28 = insertvalue { i64, i64, i64, i64 } %27, i64 %3, 2
  %29 = add i64 %.0, -1
  %30 = sub i64 %29, %.035
  %31 = sub i64 0, %3
  %32 = sdiv i64 %30, %31
  %33 = add i64 %32, 1
  %34 = insertvalue { i64, i64, i64, i64 } %28, i64 %33, 3
  ret { i64, i64, i64, i64 } %34

; <label>:35:                                     ; preds = %21
  %36 = icmp slt i64 %.0, %.035
  br i1 %36, label %37, label %46

; <label>:37:                                     ; preds = %35
  %38 = insertvalue { i64, i64, i64, i64 } undef, i64 %.0, 0
  %39 = insertvalue { i64, i64, i64, i64 } %38, i64 %.035, 1
  %40 = insertvalue { i64, i64, i64, i64 } %39, i64 %3, 2
  %41 = xor i64 %.0, -1
  %42 = add i64 %.035, %41
  %43 = sdiv i64 %42, %3
  %44 = add i64 %43, 1
  %45 = insertvalue { i64, i64, i64, i64 } %40, i64 %44, 3
  ret { i64, i64, i64, i64 } %45

; <label>:46:                                     ; preds = %35, %23
  %47 = insertvalue { i64, i64, i64, i64 } undef, i64 %.0, 0
  %48 = insertvalue { i64, i64, i64, i64 } %47, i64 %.035, 1
  %49 = insertvalue { i64, i64, i64, i64 } %48, i64 %3, 2
  %50 = insertvalue { i64, i64, i64, i64 } %49, i64 0, 3
  ret { i64, i64, i64, i64 } %50
}

; Function Attrs: noinline nounwind uwtable
define { i64, i8* } @"str::__getitem__.str.str.rslice"({ i64, i8* }, { i64 }) local_unnamed_addr #3 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.fca.1.extract59 = extractvalue { i64, i8* } %0, 1
  %.fca.0.extract54 = extractvalue { i64 } %1, 0
  %2 = call i64 @"len[str].int.str"({ i64, i8* } %0)
  %3 = insertvalue { i1, i64 } { i1 true, i64 undef }, i64 %.fca.0.extract54, 1
  %4 = call { i64, i64, i64, i64 } @"slice::adjust_indices.tuple[int,int,int,int].int.optional[int].optional[int].optional[int]"(i64 %2, { i1, i64 } %3, { i1, i64 } { i1 false, i64 undef }, { i1, i64 } { i1 false, i64 undef })
  %.fca.0.extract = extractvalue { i64, i64, i64, i64 } %4, 0
  %.fca.3.extract = extractvalue { i64, i64, i64, i64 } %4, 3
  %5 = getelementptr i8, i8* %.fca.1.extract59, i64 %.fca.0.extract
  %6 = insertvalue { i64, i8* } undef, i8* %5, 1
  %7 = insertvalue { i64, i8* } %6, i64 %.fca.3.extract, 0
  ret { i64, i8* } %7
}

; Function Attrs: noinline uwtable
define void @"dict[str,str]::__setitem__.void.dict[str,str].str.str"(i8* nocapture, { i64, i8* }, { i64, i8* }) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %3 = call { i64, i64 } @"dict[str,str]::_kh_put.tuple[int,int].dict[str,str].str"(i8* %0, { i64, i8* } %1)
  %.fca.1.extract = extractvalue { i64, i64 } %3, 1
  %.elt37 = getelementptr inbounds i8, i8* %0, i64 48
  %4 = bitcast i8* %.elt37 to { i64, i8* }**
  %.unpack38 = load { i64, i8* }*, { i64, i8* }** %4, align 8
  %.repack = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack38, i64 %.fca.1.extract, i32 0
  %.elt = extractvalue { i64, i8* } %2, 0
  store i64 %.elt, i64* %.repack, align 8
  %.repack39 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack38, i64 %.fca.1.extract, i32 1
  %.elt40 = extractvalue { i64, i8* } %2, 1
  store i8* %.elt40, i8** %.repack39, align 8
  ret void
}

; Function Attrs: noinline uwtable
define { i64, i64 } @"dict[str,str]::_kh_put.tuple[int,int].dict[str,str].str"(i8* nocapture, { i64, i8* }) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.elt = bitcast i8* %0 to i64*
  %.unpack = load i64, i64* %.elt, align 8
  %.elt88 = getelementptr inbounds i8, i8* %0, i64 16
  %2 = bitcast i8* %.elt88 to i64*
  %.unpack89 = load i64, i64* %2, align 8
  %.elt90 = getelementptr inbounds i8, i8* %0, i64 24
  %3 = bitcast i8* %.elt90 to i64*
  %.unpack91 = load i64, i64* %3, align 8
  %4 = icmp slt i64 %.unpack89, %.unpack91
  br i1 %4, label %13, label %5

; <label>:5:                                      ; preds = %preamble
  %.elt86 = getelementptr inbounds i8, i8* %0, i64 8
  %6 = bitcast i8* %.elt86 to i64*
  %.unpack87 = load i64, i64* %6, align 8
  %7 = shl i64 %.unpack87, 1
  %8 = icmp sgt i64 %.unpack, %7
  br i1 %8, label %9, label %11

; <label>:9:                                      ; preds = %5
  %10 = add i64 %.unpack, -1
  call void @"dict[str,str]::_kh_resize.void.dict[str,str].int"(i8* nonnull %0, i64 %10)
  br label %13

; <label>:11:                                     ; preds = %5
  %12 = add i64 %.unpack, 1
  call void @"dict[str,str]::_kh_resize.void.dict[str,str].int"(i8* nonnull %0, i64 %12)
  br label %13

; <label>:13:                                     ; preds = %11, %9, %preamble
  %.unpack99 = load i64, i64* %.elt, align 8
  %14 = add i64 %.unpack99, -1
  %15 = call i64 @"_dict_hash[str].int.str"({ i64, i8* } %1)
  %16 = and i64 %15, %14
  %.elt120 = getelementptr inbounds i8, i8* %0, i64 32
  %17 = bitcast i8* %.elt120 to i32**
  %.unpack121 = load i32*, i32** %17, align 8
  %18 = call i64 @"__ac_isempty.int.ptr[u32].int"(i32* %.unpack121, i64 %16)
  %19 = icmp eq i64 %18, 0
  br i1 %19, label %.lr.ph, label %.thread368

.lr.ph:                                           ; preds = %13
  %.elt359 = getelementptr inbounds i8, i8* %0, i64 40
  %20 = bitcast i8* %.elt359 to { i64, i8* }**
  br label %23

while:                                            ; preds = %body
  %21 = call i64 @"__ac_isempty.int.ptr[u32].int"(i32* %.unpack121, i64 %34)
  %22 = icmp eq i64 %21, 0
  br i1 %22, label %23, label %exit

; <label>:23:                                     ; preds = %while, %.lr.ph
  %.080372 = phi i64 [ 0, %.lr.ph ], [ %32, %while ]
  %.081371 = phi i64 [ %16, %.lr.ph ], [ %34, %while ]
  %.082370 = phi i64 [ %.unpack99, %.lr.ph ], [ %spec.select, %while ]
  %24 = call i64 @"__ac_isdel.int.ptr[u32].int"(i32* %.unpack121, i64 %.081371)
  %25 = icmp eq i64 %24, 0
  br i1 %25, label %26, label %body

; <label>:26:                                     ; preds = %23
  %.unpack360 = load { i64, i8* }*, { i64, i8* }** %20, align 8
  %.elt363 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack360, i64 %.081371, i32 0
  %.unpack364 = load i64, i64* %.elt363, align 8
  %27 = insertvalue { i64, i8* } undef, i64 %.unpack364, 0
  %.elt365 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack360, i64 %.081371, i32 1
  %.unpack366 = load i8*, i8** %.elt365, align 8
  %28 = insertvalue { i64, i8* } %27, i8* %.unpack366, 1
  %29 = call i8 @"str::__ne__.bool.str.str"({ i64, i8* } %28, { i64, i8* } %1)
  %30 = and i8 %29, 1
  %31 = icmp eq i8 %30, 0
  br i1 %31, label %exit, label %body

body:                                             ; preds = %26, %23
  %spec.select = select i1 %25, i64 %.082370, i64 %.081371
  %32 = add i64 %.080372, 1
  %33 = add i64 %32, %.081371
  %34 = and i64 %33, %14
  %35 = icmp eq i64 %34, %16
  br i1 %35, label %exit, label %while

exit:                                             ; preds = %body, %26, %while
  %.084.ph = phi i64 [ %.unpack99, %26 ], [ %spec.select, %body ], [ %.unpack99, %while ]
  %.2.ph = phi i64 [ %.082370, %26 ], [ %spec.select, %body ], [ %spec.select, %while ]
  %.1.ph = phi i64 [ %.081371, %26 ], [ %16, %body ], [ %34, %while ]
  %.unpack141 = load i64, i64* %.elt, align 8
  %36 = icmp eq i64 %.084.ph, %.unpack141
  br i1 %36, label %37, label %.thread368

; <label>:37:                                     ; preds = %exit
  %38 = call i64 @"__ac_isempty.int.ptr[u32].int"(i32* %.unpack121, i64 %.1.ph)
  %39 = icmp ne i64 %38, 0
  %40 = icmp ne i64 %.2.ph, %.084.ph
  %or.cond = and i1 %40, %39
  %spec.select369 = select i1 %or.cond, i64 %.2.ph, i64 %.1.ph
  br label %.thread368

.thread368:                                       ; preds = %37, %exit, %13
  %.185 = phi i64 [ %.084.ph, %exit ], [ %16, %13 ], [ %spec.select369, %37 ]
  %41 = call i64 @"__ac_isempty.int.ptr[u32].int"(i32* %.unpack121, i64 %.185)
  %42 = icmp eq i64 %41, 0
  br i1 %42, label %48, label %43

; <label>:43:                                     ; preds = %.thread368
  %.elt254 = getelementptr inbounds i8, i8* %0, i64 40
  %44 = bitcast i8* %.elt254 to { i64, i8* }**
  %.unpack255 = load { i64, i8* }*, { i64, i8* }** %44, align 8
  %.repack258 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack255, i64 %.185, i32 0
  %.elt259 = extractvalue { i64, i8* } %1, 0
  store i64 %.elt259, i64* %.repack258, align 8
  %.repack260 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack255, i64 %.185, i32 1
  %.elt261 = extractvalue { i64, i8* } %1, 1
  store i8* %.elt261, i8** %.repack260, align 8
  %.unpack271 = load i32*, i32** %17, align 8
  call void @"__ac_set_isboth_false.void.ptr[u32].int"(i32* %.unpack271, i64 %.185)
  %.elt278 = getelementptr inbounds i8, i8* %0, i64 8
  %45 = bitcast i8* %.elt278 to i64*
  %.unpack279 = load i64, i64* %45, align 8
  %.unpack281 = load i64, i64* %2, align 8
  %46 = add i64 %.unpack279, 1
  %47 = add i64 %.unpack281, 1
  store i64 %46, i64* %45, align 8
  store i64 %47, i64* %2, align 8
  br label %55

; <label>:48:                                     ; preds = %.thread368
  %49 = call i64 @"__ac_isdel.int.ptr[u32].int"(i32* %.unpack121, i64 %.185)
  %50 = icmp eq i64 %49, 0
  br i1 %50, label %55, label %51

; <label>:51:                                     ; preds = %48
  %.elt192 = getelementptr inbounds i8, i8* %0, i64 40
  %52 = bitcast i8* %.elt192 to { i64, i8* }**
  %.unpack193 = load { i64, i8* }*, { i64, i8* }** %52, align 8
  %.repack = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack193, i64 %.185, i32 0
  %.elt196 = extractvalue { i64, i8* } %1, 0
  store i64 %.elt196, i64* %.repack, align 8
  %.repack197 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack193, i64 %.185, i32 1
  %.elt198 = extractvalue { i64, i8* } %1, 1
  store i8* %.elt198, i8** %.repack197, align 8
  %.unpack208 = load i32*, i32** %17, align 8
  call void @"__ac_set_isboth_false.void.ptr[u32].int"(i32* %.unpack208, i64 %.185)
  %.elt215 = getelementptr inbounds i8, i8* %0, i64 8
  %53 = bitcast i8* %.elt215 to i64*
  %.unpack216 = load i64, i64* %53, align 8
  %54 = add i64 %.unpack216, 1
  store i64 %54, i64* %53, align 8
  br label %55

; <label>:55:                                     ; preds = %51, %48, %43
  %.0 = phi i64 [ 1, %43 ], [ 2, %51 ], [ 0, %48 ]
  %56 = insertvalue { i64, i64 } undef, i64 %.0, 0
  %57 = insertvalue { i64, i64 } %56, i64 %.185, 1
  ret { i64, i64 } %57
}

; Function Attrs: noinline uwtable
define void @"dict[str,str]::_kh_resize.void.dict[str,str].int"(i8* nocapture, i64) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %2 = add i64 %1, -1
  %3 = ashr i64 %2, 1
  %4 = or i64 %3, %2
  %5 = ashr i64 %4, 2
  %6 = or i64 %5, %4
  %7 = ashr i64 %6, 4
  %8 = or i64 %7, %6
  %9 = ashr i64 %8, 8
  %10 = or i64 %9, %8
  %11 = ashr i64 %10, 16
  %12 = or i64 %11, %10
  %13 = ashr i64 %12, 32
  %14 = or i64 %13, %12
  %15 = add i64 %14, 1
  %16 = icmp sgt i64 %15, 4
  %spec.store.select = select i1 %16, i64 %15, i64 4
  %.elt183 = getelementptr inbounds i8, i8* %0, i64 8
  %17 = bitcast i8* %.elt183 to i64*
  %.unpack184 = load i64, i64* %17, align 8
  %18 = sitofp i64 %spec.store.select to double
  %19 = fmul double %18, 7.700000e-01
  %20 = fadd double %19, 5.000000e-01
  %21 = fptosi double %20 to i64
  %22 = icmp slt i64 %.unpack184, %21
  br i1 %22, label %23, label %67

; <label>:23:                                     ; preds = %preamble
  %24 = call i64 @"__ac_fsize[int].int.int"(i64 %spec.store.select)
  %25 = shl i64 %24, 2
  %26 = call i8* @seq_alloc_atomic(i64 %25)
  %27 = bitcast i8* %26 to i32*
  %28 = icmp sgt i64 %24, 0
  br i1 %28, label %body.lr.ph, label %exit

body.lr.ph:                                       ; preds = %23
  call void @llvm.memset.p0i8.i64(i8* %26, i8 -86, i64 %25, i32 4, i1 false)
  br label %exit

exit:                                             ; preds = %body.lr.ph, %23
  %.elt = bitcast i8* %0 to i64*
  %.unpack = load i64, i64* %.elt, align 8
  %29 = icmp slt i64 %.unpack, %spec.store.select
  %.elt203 = getelementptr inbounds i8, i8* %0, i64 40
  %30 = bitcast i8* %.elt203 to i8**
  br i1 %29, label %31, label %exit._crit_edge

; <label>:31:                                     ; preds = %exit
  %.unpack204436 = load i8*, i8** %30, align 8
  %32 = shl i64 %spec.store.select, 4
  %33 = call i8* @"realloc.ptr[byte].ptr[byte].int"(i8* %.unpack204436, i64 %32)
  %.elt449 = getelementptr inbounds i8, i8* %0, i64 48
  %34 = bitcast i8* %.elt449 to i8**
  %.unpack450500 = load i8*, i8** %34, align 8
  store i8* %33, i8** %30, align 8
  %35 = call i8* @"realloc.ptr[byte].ptr[byte].int"(i8* %.unpack450500, i64 %32)
  store i8* %35, i8** %34, align 8
  %.unpack208514.pre = load i64, i64* %.elt, align 8
  br label %exit._crit_edge

exit._crit_edge:                                  ; preds = %31, %exit
  %.unpack208514 = phi i64 [ %.unpack208514.pre, %31 ], [ %.unpack, %exit ]
  %36 = icmp eq i64 %.unpack208514, 0
  %.pre = getelementptr inbounds i8, i8* %0, i64 32
  br i1 %36, label %exit9.thread, label %body2.lr.ph

body2.lr.ph:                                      ; preds = %exit._crit_edge
  %37 = bitcast i8* %.pre to i32**
  %38 = bitcast i8* %.elt203 to { i64, i8* }**
  %.elt322 = getelementptr inbounds i8, i8* %0, i64 48
  %39 = bitcast i8* %.elt322 to { i64, i8* }**
  %40 = add nsw i64 %spec.store.select, -1
  br label %body2

body2:                                            ; preds = %while1, %body2.lr.ph
  %.unpack208535 = phi i64 [ %.unpack208514, %body2.lr.ph ], [ %.unpack208, %while1 ]
  %.1516 = phi i64 [ 0, %body2.lr.ph ], [ %56, %while1 ]
  %.unpack216 = load i32*, i32** %37, align 8
  %41 = call i64 @"__ac_iseither.int.ptr[u32].int"(i32* %.unpack216, i64 %.1516)
  %42 = icmp eq i64 %41, 0
  br i1 %42, label %43, label %while1

; <label>:43:                                     ; preds = %body2
  %.unpack321 = load { i64, i8* }*, { i64, i8* }** %38, align 8
  %.unpack323 = load { i64, i8* }*, { i64, i8* }** %39, align 8
  %.elt324 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack321, i64 %.1516, i32 0
  %.unpack325 = load i64, i64* %.elt324, align 8
  %.elt326 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack321, i64 %.1516, i32 1
  %.unpack327 = load i8*, i8** %.elt326, align 8
  %.elt328 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack323, i64 %.1516, i32 0
  %.unpack329 = load i64, i64* %.elt328, align 8
  %.elt330 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack323, i64 %.1516, i32 1
  %.unpack331 = load i8*, i8** %.elt330, align 8
  call void @"__ac_set_isdel_true.void.ptr[u32].int"(i32* %.unpack216, i64 %.1516)
  br label %while3

while3:                                           ; preds = %55, %43
  %.sroa.086.0 = phi i64 [ %.unpack329, %43 ], [ %.unpack416, %55 ]
  %.sroa.4.0 = phi i8* [ %.unpack331, %43 ], [ %.unpack418, %55 ]
  %.sroa.0104.0 = phi i64 [ %.unpack325, %43 ], [ %.unpack395, %55 ]
  %.sroa.4106.0 = phi i8* [ %.unpack327, %43 ], [ %.unpack397, %55 ]
  %.fca.0.insert91 = insertvalue { i64, i8* } undef, i64 %.sroa.0104.0, 0
  %.fca.1.insert93 = insertvalue { i64, i8* } %.fca.0.insert91, i8* %.sroa.4106.0, 1
  %44 = call i64 @"_dict_hash[str].int.str"({ i64, i8* } %.fca.1.insert93)
  %.0181511 = and i64 %44, %40
  %45 = call i64 @"__ac_isempty.int.ptr[u32].int"(i32* %27, i64 %.0181511)
  %46 = icmp eq i64 %45, 0
  br i1 %46, label %body6.preheader, label %exit7

body6.preheader:                                  ; preds = %while3
  br label %body6

body6:                                            ; preds = %body6, %body6.preheader
  %.0181513 = phi i64 [ %.0181, %body6 ], [ %.0181511, %body6.preheader ]
  %.0182512 = phi i64 [ %47, %body6 ], [ 0, %body6.preheader ]
  %47 = add i64 %.0182512, 1
  %48 = add i64 %47, %.0181513
  %.0181 = and i64 %48, %40
  %49 = call i64 @"__ac_isempty.int.ptr[u32].int"(i32* %27, i64 %.0181)
  %50 = icmp eq i64 %49, 0
  br i1 %50, label %body6, label %exit7

exit7:                                            ; preds = %body6, %while3
  %.0181.lcssa = phi i64 [ %.0181511, %while3 ], [ %.0181, %body6 ]
  call void @"__ac_set_isempty_false.void.ptr[u32].int"(i32* %27, i64 %.0181.lcssa)
  %.unpack333 = load i64, i64* %.elt, align 8
  %51 = icmp slt i64 %.0181.lcssa, %.unpack333
  br i1 %51, label %52, label %.thread

.thread:                                          ; preds = %exit7
  %.unpack391497 = load { i64, i8* }*, { i64, i8* }** %38, align 8
  br label %exit8

; <label>:52:                                     ; preds = %exit7
  %.unpack341 = load i32*, i32** %37, align 8
  %53 = call i64 @"__ac_iseither.int.ptr[u32].int"(i32* %.unpack341, i64 %.0181.lcssa)
  %54 = icmp eq i64 %53, 0
  %.unpack391 = load { i64, i8* }*, { i64, i8* }** %38, align 8
  br i1 %54, label %55, label %exit8

; <label>:55:                                     ; preds = %52
  %.elt394 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack391, i64 %.0181.lcssa, i32 0
  %.unpack395 = load i64, i64* %.elt394, align 8
  %.elt396 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack391, i64 %.0181.lcssa, i32 1
  %.unpack397 = load i8*, i8** %.elt396, align 8
  store i64 %.sroa.0104.0, i64* %.elt394, align 8
  store i8* %.sroa.4106.0, i8** %.elt396, align 8
  %.unpack414 = load { i64, i8* }*, { i64, i8* }** %39, align 8
  %.elt415 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack414, i64 %.0181.lcssa, i32 0
  %.unpack416 = load i64, i64* %.elt415, align 8
  %.elt417 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack414, i64 %.0181.lcssa, i32 1
  %.unpack418 = load i8*, i8** %.elt417, align 8
  store i64 %.sroa.086.0, i64* %.elt415, align 8
  store i8* %.sroa.4.0, i8** %.elt417, align 8
  %.unpack431 = load i32*, i32** %37, align 8
  call void @"__ac_set_isdel_true.void.ptr[u32].int"(i32* %.unpack431, i64 %.0181.lcssa)
  br label %while3

exit8:                                            ; preds = %52, %.thread
  %.unpack391498 = phi { i64, i8* }* [ %.unpack391497, %.thread ], [ %.unpack391, %52 ]
  %.repack360 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack391498, i64 %.0181.lcssa, i32 0
  store i64 %.sroa.0104.0, i64* %.repack360, align 8
  %.repack361 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack391498, i64 %.0181.lcssa, i32 1
  store i8* %.sroa.4106.0, i8** %.repack361, align 8
  %.unpack376 = load { i64, i8* }*, { i64, i8* }** %39, align 8
  %.repack377 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack376, i64 %.0181.lcssa, i32 0
  store i64 %.sroa.086.0, i64* %.repack377, align 8
  %.repack378 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack376, i64 %.0181.lcssa, i32 1
  store i8* %.sroa.4.0, i8** %.repack378, align 8
  %.unpack208.pre = load i64, i64* %.elt, align 8
  br label %while1

while1:                                           ; preds = %exit8, %body2
  %.unpack208 = phi i64 [ %.unpack208535, %body2 ], [ %.unpack208.pre, %exit8 ]
  %56 = add i64 %.1516, 1
  %57 = icmp eq i64 %56, %.unpack208
  br i1 %57, label %exit9, label %body2

exit9:                                            ; preds = %while1
  %58 = icmp sgt i64 %.unpack208, %spec.store.select
  br i1 %58, label %59, label %exit9.thread

; <label>:59:                                     ; preds = %exit9
  %.unpack218309.lcssa = load i8*, i8** %30, align 8
  %60 = shl i64 %spec.store.select, 4
  %61 = call i8* @"realloc.ptr[byte].ptr[byte].int"(i8* %.unpack218309.lcssa, i64 %60)
  %62 = bitcast i8* %.elt322 to i8**
  %.unpack263499 = load i8*, i8** %62, align 8
  store i8* %61, i8** %30, align 8
  %63 = call i8* @"realloc.ptr[byte].ptr[byte].int"(i8* %.unpack263499, i64 %60)
  store i8* %63, i8** %62, align 8
  br label %exit9.thread

exit9.thread:                                     ; preds = %59, %exit9, %exit._crit_edge
  %.unpack224 = load i64, i64* %17, align 8
  store i64 %spec.store.select, i64* %.elt, align 8
  %.repack238 = getelementptr inbounds i8, i8* %0, i64 16
  %64 = bitcast i8* %.repack238 to i64*
  store i64 %.unpack224, i64* %64, align 8
  %.repack240 = getelementptr inbounds i8, i8* %0, i64 24
  %65 = bitcast i8* %.repack240 to i64*
  store i64 %21, i64* %65, align 8
  %66 = bitcast i8* %.pre to i8**
  store i8* %26, i8** %66, align 8
  br label %67

; <label>:67:                                     ; preds = %exit9.thread, %preamble
  ret void
}

; Function Attrs: noinline norecurse nounwind readnone uwtable
define i64 @"__ac_fsize[int].int.int"(i64) local_unnamed_addr #2 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %1 = icmp slt i64 %0, 16
  %2 = ashr i64 %0, 4
  %3 = select i1 %1, i64 1, i64 %2
  ret i64 %3
}

; Function Attrs: noinline norecurse nounwind readnone uwtable
define i64 @"sizeof[str].int"() local_unnamed_addr #2 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  ret i64 16
}

; Function Attrs: noinline norecurse nounwind readnone uwtable
define i64 @seq.magic.__elemsize__.3() local_unnamed_addr #2 {
entry:
  ret i64 16
}

; Function Attrs: noinline norecurse nounwind readonly uwtable
define i64 @"__ac_iseither.int.ptr[u32].int"(i32* nocapture readonly, i64) local_unnamed_addr #8 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %2 = ashr i64 %1, 4
  %3 = getelementptr i32, i32* %0, i64 %2
  %4 = load i32, i32* %3, align 4
  %.tr = trunc i64 %1 to i32
  %5 = shl i32 %.tr, 1
  %6 = and i32 %5, 30
  %7 = lshr i32 %4, %6
  %8 = and i32 %7, 3
  %9 = zext i32 %8 to i64
  ret i64 %9
}

; Function Attrs: noinline norecurse nounwind uwtable
define void @"__ac_set_isdel_true.void.ptr[u32].int"(i32* nocapture, i64) local_unnamed_addr #7 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %2 = ashr i64 %1, 4
  %3 = getelementptr i32, i32* %0, i64 %2
  %4 = load i32, i32* %3, align 4
  %5 = shl i64 %1, 1
  %6 = and i64 %5, 30
  %7 = shl i64 1, %6
  %8 = trunc i64 %7 to i32
  %9 = or i32 %4, %8
  store i32 %9, i32* %3, align 4
  ret void
}

; Function Attrs: noinline uwtable
define i64 @"_dict_hash[str].int.str"({ i64, i8* }) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %1 = call i64 @"hash[str].int.str"({ i64, i8* } %0)
  %2 = ashr i64 %1, 33
  %3 = xor i64 %2, %1
  %4 = shl i64 %1, 11
  %5 = xor i64 %3, %4
  ret i64 %5
}

; Function Attrs: noinline uwtable
define i64 @"hash[str].int.str"({ i64, i8* }) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %1 = call i64 @"str::__hash__.int.str"({ i64, i8* } %0)
  ret i64 %1
}

; Function Attrs: noinline uwtable
define i64 @"str::__hash__.int.str"({ i64, i8* }) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.fca.0.extract13 = extractvalue { i64, i8* } %0, 0
  %.fca.1.extract14 = extractvalue { i64, i8* } %0, 1
  %1 = call { i64, i64, i64 } @"range::__init__.range.range.int"({ i64, i64, i64 } undef, i64 %.fca.0.extract13)
  %2 = call i8* @"range::__iter__.generator[int].range"({ i64, i64, i64 } %1)
  %3 = bitcast i8* %2 to void (i8*)**
  %4 = load void (i8*)*, void (i8*)** %3, align 8
  call fastcc void %4(i8* %2)
  %5 = bitcast i8* %2 to i8**
  %6 = load i8*, i8** %5, align 8
  %7 = icmp eq i8* %6, null
  br i1 %7, label %cleanup, label %body.lr.ph

body.lr.ph:                                       ; preds = %preamble
  %8 = getelementptr inbounds i8, i8* %2, i64 16
  %9 = bitcast i8* %8 to i64*
  br label %body

body:                                             ; preds = %body, %body.lr.ph
  %.in = phi i8* [ %6, %body.lr.ph ], [ %17, %body ]
  %.028 = phi i64 [ 0, %body.lr.ph ], [ %16, %body ]
  %10 = bitcast i8* %.in to void (i8*)*
  %11 = load i64, i64* %9, align 8
  %12 = mul i64 %.028, 31
  %13 = getelementptr i8, i8* %.fca.1.extract14, i64 %11
  %14 = load i8, i8* %13, align 1
  %15 = zext i8 %14 to i64
  %16 = add i64 %12, %15
  call fastcc void %10(i8* %2)
  %17 = load i8*, i8** %5, align 8
  %18 = icmp eq i8* %17, null
  br i1 %18, label %cleanup, label %body

cleanup:                                          ; preds = %body, %preamble
  %.0.lcssa = phi i64 [ 0, %preamble ], [ %16, %body ]
  %19 = getelementptr inbounds i8, i8* %2, i64 8
  %20 = bitcast i8* %19 to void (i8*)**
  %21 = load void (i8*)*, void (i8*)** %20, align 8
  call fastcc void %21(i8* nonnull %2)
  ret i64 %.0.lcssa
}

; Function Attrs: noinline norecurse nounwind readnone uwtable
define { i64, i64, i64 } @"range::__init__.range.range.int"({ i64, i64, i64 }, i64) local_unnamed_addr #2 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %2 = insertvalue { i64, i64, i64 } { i64 0, i64 undef, i64 undef }, i64 %1, 1
  %3 = insertvalue { i64, i64, i64 } %2, i64 1, 2
  ret { i64, i64, i64 } %3
}

; Function Attrs: noinline nounwind uwtable
define noalias i8* @"range::__iter__.generator[int].range"({ i64, i64, i64 }) local_unnamed_addr #3 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %1 = call i8* @seq_alloc(i64 72)
  %resume.addr = bitcast i8* %1 to void (%"range::__iter__.generator[int].range.Frame"*)**
  store void (%"range::__iter__.generator[int].range.Frame"*)* @"range::__iter__.generator[int].range.resume", void (%"range::__iter__.generator[int].range.Frame"*)** %resume.addr, align 8
  %destroy.addr = getelementptr inbounds i8, i8* %1, i64 8
  %2 = bitcast i8* %destroy.addr to void (%"range::__iter__.generator[int].range.Frame"*)**
  store void (%"range::__iter__.generator[int].range.Frame"*)* @"range::__iter__.generator[int].range.destroy", void (%"range::__iter__.generator[int].range.Frame"*)** %2, align 8
  %.fca.0.extract45 = extractvalue { i64, i64, i64 } %0, 0
  %.fca.0.extract45.spill.addr = getelementptr inbounds i8, i8* %1, i64 32
  %3 = bitcast i8* %.fca.0.extract45.spill.addr to i64*
  store i64 %.fca.0.extract45, i64* %3, align 8
  %.fca.1.extract46 = extractvalue { i64, i64, i64 } %0, 1
  %.fca.1.extract46.spill.addr = getelementptr inbounds i8, i8* %1, i64 40
  %4 = bitcast i8* %.fca.1.extract46.spill.addr to i64*
  store i64 %.fca.1.extract46, i64* %4, align 8
  %.fca.2.extract47 = extractvalue { i64, i64, i64 } %0, 2
  %.fca.2.extract47.spill.addr = getelementptr inbounds i8, i8* %1, i64 48
  %5 = bitcast i8* %.fca.2.extract47.spill.addr to i64*
  store i64 %.fca.2.extract47, i64* %5, align 8
  %index.addr115 = getelementptr inbounds i8, i8* %1, i64 24
  %6 = bitcast i8* %index.addr115 to i2*
  store i2 0, i2* %6, align 1
  ret i8* %1
}

; Function Attrs: noinline norecurse nounwind readonly uwtable
define i64 @"__ac_isempty.int.ptr[u32].int"(i32* nocapture readonly, i64) local_unnamed_addr #8 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %2 = ashr i64 %1, 4
  %3 = getelementptr i32, i32* %0, i64 %2
  %4 = load i32, i32* %3, align 4
  %.tr = trunc i64 %1 to i32
  %5 = shl i32 %.tr, 1
  %6 = and i32 %5, 30
  %7 = lshr i32 %4, %6
  %8 = and i32 %7, 2
  %9 = zext i32 %8 to i64
  ret i64 %9
}

; Function Attrs: noinline norecurse nounwind uwtable
define void @"__ac_set_isempty_false.void.ptr[u32].int"(i32* nocapture, i64) local_unnamed_addr #7 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %2 = ashr i64 %1, 4
  %3 = getelementptr i32, i32* %0, i64 %2
  %4 = load i32, i32* %3, align 4
  %5 = shl i64 %1, 1
  %6 = and i64 %5, 30
  %7 = shl i64 2, %6
  %8 = trunc i64 %7 to i32
  %9 = xor i32 %8, -1
  %10 = and i32 %4, %9
  store i32 %10, i32* %3, align 4
  ret void
}

; Function Attrs: noinline norecurse nounwind readonly uwtable
define i64 @"__ac_isdel.int.ptr[u32].int"(i32* nocapture readonly, i64) local_unnamed_addr #8 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %2 = ashr i64 %1, 4
  %3 = getelementptr i32, i32* %0, i64 %2
  %4 = load i32, i32* %3, align 4
  %.tr = trunc i64 %1 to i32
  %5 = shl i32 %.tr, 1
  %6 = and i32 %5, 30
  %7 = lshr i32 %4, %6
  %8 = and i32 %7, 1
  %9 = zext i32 %8 to i64
  ret i64 %9
}

; Function Attrs: noinline norecurse nounwind readonly uwtable
define i8 @"str::__ne__.bool.str.str"({ i64, i8* }, { i64, i8* }) local_unnamed_addr #8 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %2 = call i8 @"str::__eq__.bool.str.str"({ i64, i8* } %0, { i64, i8* } %1)
  %3 = and i8 %2, 1
  %4 = xor i8 %3, 1
  ret i8 %4
}

; Function Attrs: noinline norecurse nounwind uwtable
define void @"__ac_set_isboth_false.void.ptr[u32].int"(i32* nocapture, i64) local_unnamed_addr #7 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %2 = ashr i64 %1, 4
  %3 = getelementptr i32, i32* %0, i64 %2
  %4 = load i32, i32* %3, align 4
  %5 = shl i64 %1, 1
  %6 = and i64 %5, 30
  %7 = shl i64 3, %6
  %8 = trunc i64 %7 to i32
  %9 = xor i32 %8, -1
  %10 = and i32 %4, %9
  store i32 %10, i32* %3, align 4
  ret void
}

; Function Attrs: noinline uwtable
define i8 @"dict[str,str]::__contains__.bool.dict[str,str].str"(i8* nocapture readonly, { i64, i8* }) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %2 = call i64 @"dict[str,str]::_kh_get.int.dict[str,str].str"(i8* %0, { i64, i8* } %1)
  %3 = call i64 @"dict[str,str]::_kh_end.int.dict[str,str]"(i8* %0)
  %4 = icmp ne i64 %2, %3
  %5 = zext i1 %4 to i8
  ret i8 %5
}

; Function Attrs: noinline uwtable
define i64 @"dict[str,str]::_kh_get.int.dict[str,str].str"(i8* nocapture readonly, { i64, i8* }) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.elt = bitcast i8* %0 to i64*
  %.unpack = load i64, i64* %.elt, align 8
  %2 = icmp eq i64 %.unpack, 0
  br i1 %2, label %30, label %3

; <label>:3:                                      ; preds = %preamble
  %4 = add i64 %.unpack, -1
  %5 = call i64 @"_dict_hash[str].int.str"({ i64, i8* } %1)
  %6 = and i64 %5, %4
  %.elt51 = getelementptr inbounds i8, i8* %0, i64 32
  %7 = bitcast i8* %.elt51 to i32**
  %.unpack52134 = load i32*, i32** %7, align 8
  %8 = call i64 @"__ac_isempty.int.ptr[u32].int"(i32* %.unpack52134, i64 %6)
  %9 = icmp eq i64 %8, 0
  br i1 %9, label %.lr.ph, label %exit

.lr.ph:                                           ; preds = %3
  %.elt123 = getelementptr inbounds i8, i8* %0, i64 40
  %10 = bitcast i8* %.elt123 to { i64, i8* }**
  br label %13

while:                                            ; preds = %body
  %11 = call i64 @"__ac_isempty.int.ptr[u32].int"(i32* %.unpack52134, i64 %24)
  %12 = icmp eq i64 %11, 0
  br i1 %12, label %13, label %exit

; <label>:13:                                     ; preds = %while, %.lr.ph
  %.0136 = phi i64 [ 0, %.lr.ph ], [ %22, %while ]
  %.030135 = phi i64 [ %6, %.lr.ph ], [ %24, %while ]
  %14 = call i64 @"__ac_isdel.int.ptr[u32].int"(i32* %.unpack52134, i64 %.030135)
  %15 = icmp eq i64 %14, 0
  br i1 %15, label %16, label %body

; <label>:16:                                     ; preds = %13
  %.unpack124 = load { i64, i8* }*, { i64, i8* }** %10, align 8
  %.elt127 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack124, i64 %.030135, i32 0
  %.unpack128 = load i64, i64* %.elt127, align 8
  %17 = insertvalue { i64, i8* } undef, i64 %.unpack128, 0
  %.elt129 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack124, i64 %.030135, i32 1
  %.unpack130 = load i8*, i8** %.elt129, align 8
  %18 = insertvalue { i64, i8* } %17, i8* %.unpack130, 1
  %19 = call i8 @"str::__ne__.bool.str.str"({ i64, i8* } %18, { i64, i8* } %1)
  %20 = and i8 %19, 1
  %21 = icmp eq i8 %20, 0
  br i1 %21, label %exit, label %body

body:                                             ; preds = %16, %13
  %22 = add i64 %.0136, 1
  %23 = add i64 %22, %.030135
  %24 = and i64 %23, %4
  %25 = icmp eq i64 %24, %6
  br i1 %25, label %26, label %while

; <label>:26:                                     ; preds = %body
  %.unpack86 = load i64, i64* %.elt, align 8
  ret i64 %.unpack86

exit:                                             ; preds = %16, %while, %3
  %.030.lcssa = phi i64 [ %6, %3 ], [ %.030135, %16 ], [ %24, %while ]
  %27 = call i64 @"__ac_iseither.int.ptr[u32].int"(i32* %.unpack52134, i64 %.030.lcssa)
  %28 = icmp eq i64 %27, 0
  br i1 %28, label %30, label %29

; <label>:29:                                     ; preds = %exit
  %.unpack72 = load i64, i64* %.elt, align 8
  br label %30

; <label>:30:                                     ; preds = %29, %exit, %preamble
  %31 = phi i64 [ %.unpack72, %29 ], [ 0, %preamble ], [ %.030.lcssa, %exit ]
  ret i64 %31
}

; Function Attrs: noinline norecurse nounwind readonly uwtable
define i64 @"dict[str,str]::_kh_end.int.dict[str,str]"(i8* nocapture readonly) local_unnamed_addr #8 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.elt = bitcast i8* %0 to i64*
  %.unpack = load i64, i64* %.elt, align 8
  ret i64 %.unpack
}

; Function Attrs: noinline uwtable
define { i64, i8* } @"EnvMap::__getitem__.str.EnvMap.str"({ i8* }, { i64, i8* }) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.fca.0.extract3 = extractvalue { i8* } %0, 0
  call void @"EnvMap::_init_if_needed.void.EnvMap"({ i8* } %0)
  %2 = call { i64, i8* } @"dict[str,str]::__getitem__.str.dict[str,str].str"(i8* %.fca.0.extract3, { i64, i8* } %1)
  ret { i64, i8* } %2
}

; Function Attrs: noinline uwtable
define { i64, i8* } @"dict[str,str]::__getitem__.str.dict[str,str].str"(i8* nocapture readonly, { i64, i8* }) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %2 = call i64 @"dict[str,str]::_kh_get.int.dict[str,str].str"(i8* %0, { i64, i8* } %1)
  %3 = call i64 @"dict[str,str]::_kh_end.int.dict[str,str]"(i8* %0)
  %4 = icmp eq i64 %2, %3
  br i1 %4, label %9, label %5

; <label>:5:                                      ; preds = %preamble
  %.elt59 = getelementptr inbounds i8, i8* %0, i64 48
  %6 = bitcast i8* %.elt59 to { i64, i8* }**
  %.unpack60 = load { i64, i8* }*, { i64, i8* }** %6, align 8
  %.elt = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack60, i64 %2, i32 0
  %.unpack = load i64, i64* %.elt, align 8
  %7 = insertvalue { i64, i8* } undef, i64 %.unpack, 0
  %.elt61 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack60, i64 %2, i32 1
  %.unpack62 = load i8*, i8** %.elt61, align 8
  %8 = insertvalue { i64, i8* } %7, i8* %.unpack62, 1
  ret { i64, i8* } %8

; <label>:9:                                      ; preds = %preamble
  %10 = call { i64, i8* } @"str::__str__.str.str"({ i64, i8* } %1)
  %11 = call i8* @seq_alloc(i64 80)
  call void @"KeyError::__init__.void.KeyError.str"(i8* %11, { i64, i8* } %10)
  %.repack31.repack = getelementptr inbounds i8, i8* %11, i64 32
  %12 = bitcast i8* %.repack31.repack to i64*
  store i64 11, i64* %12, align 8
  %.repack31.repack41 = getelementptr inbounds i8, i8* %11, i64 40
  %13 = bitcast i8* %.repack31.repack41 to i8**
  store i8* getelementptr inbounds ([12 x i8], [12 x i8]* @str_literal.32, i64 0, i64 0), i8** %13, align 8
  %.repack33.repack = getelementptr inbounds i8, i8* %11, i64 48
  %14 = bitcast i8* %.repack33.repack to i64*
  store i64 73, i64* %14, align 8
  %.repack33.repack39 = getelementptr inbounds i8, i8* %11, i64 56
  %15 = bitcast i8* %.repack33.repack39 to i8**
  store i8* getelementptr inbounds ([74 x i8], [74 x i8]* @str_literal.33, i64 0, i64 0), i8** %15, align 8
  %.repack35 = getelementptr inbounds i8, i8* %11, i64 64
  %16 = bitcast i8* %.repack35 to i64*
  store i64 47, i64* %16, align 8
  %.repack37 = getelementptr inbounds i8, i8* %11, i64 72
  %17 = bitcast i8* %.repack37 to i64*
  store i64 9, i64* %17, align 8
  %18 = call i8* @seq_alloc_exc(i32 -71732691, i8* %11)
  call void @seq_throw(i8* %18)
  unreachable
}

; Function Attrs: noinline norecurse nounwind readnone uwtable
define { i64, i8* } @"str::__str__.str.str"({ i64, i8* } returned) local_unnamed_addr #2 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  ret { i64, i8* } %0
}

; Function Attrs: noinline norecurse nounwind uwtable
define void @"KeyError::__init__.void.KeyError.str"(i8* nocapture, { i64, i8* }) local_unnamed_addr #7 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.repack.repack = bitcast i8* %0 to i64*
  store i64 8, i64* %.repack.repack, align 8
  %.repack.repack19 = getelementptr inbounds i8, i8* %0, i64 8
  %2 = bitcast i8* %.repack.repack19 to i8**
  store i8* getelementptr inbounds ([9 x i8], [9 x i8]* @str_literal.29, i64 0, i64 0), i8** %2, align 8
  %.repack3.repack = getelementptr inbounds i8, i8* %0, i64 16
  %3 = bitcast i8* %.repack3.repack to i64*
  %.elt4.elt = extractvalue { i64, i8* } %1, 0
  store i64 %.elt4.elt, i64* %3, align 8
  %.repack3.repack17 = getelementptr inbounds i8, i8* %0, i64 24
  %4 = bitcast i8* %.repack3.repack17 to i8**
  %.elt4.elt18 = extractvalue { i64, i8* } %1, 1
  store i8* %.elt4.elt18, i8** %4, align 8
  %.repack5.repack = getelementptr inbounds i8, i8* %0, i64 32
  %5 = bitcast i8* %.repack5.repack to i64*
  store i64 0, i64* %5, align 8
  %.repack5.repack15 = getelementptr inbounds i8, i8* %0, i64 40
  %6 = bitcast i8* %.repack5.repack15 to i8**
  store i8* getelementptr inbounds ([1 x i8], [1 x i8]* @str_literal.30, i64 0, i64 0), i8** %6, align 8
  %.repack7.repack = getelementptr inbounds i8, i8* %0, i64 48
  %7 = bitcast i8* %.repack7.repack to i64*
  store i64 0, i64* %7, align 8
  %.repack7.repack13 = getelementptr inbounds i8, i8* %0, i64 56
  %8 = bitcast i8* %.repack7.repack13 to i8**
  store i8* getelementptr inbounds ([1 x i8], [1 x i8]* @str_literal.31, i64 0, i64 0), i8** %8, align 8
  %.repack9 = getelementptr inbounds i8, i8* %0, i64 64
  call void @llvm.memset.p0i8.i64(i8* nonnull %.repack9, i8 0, i64 16, i32 8, i1 false)
  ret void
}

; Function Attrs: noinline norecurse nounwind uwtable
define void @"dict[str,pyobj]::__init__.void.dict[str,pyobj]"(i8* nocapture) local_unnamed_addr #7 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  call void @"dict[str,pyobj]::_init.void.dict[str,pyobj]"(i8* %0)
  ret void
}

; Function Attrs: noinline norecurse nounwind uwtable
define void @"dict[str,pyobj]::_init.void.dict[str,pyobj]"(i8* nocapture) local_unnamed_addr #7 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  call void @llvm.memset.p0i8.i64(i8* %0, i8 0, i64 56, i32 8, i1 false)
  ret void
}

; Function Attrs: noinline uwtable
define i8* @"py_tuple_new.ptr[byte].int"(i64) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %1 = call i8* @"PyTuple_New.ptr[byte].int"(i64 %0)
  call void @"pyobj::exc_check.void"()
  ret i8* %1
}

; Function Attrs: noinline uwtable
define i8* @"PyTuple_New.ptr[byte].int"(i64) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.unpack = load i64, i64* @seq.var.1847.0, align 8
  %1 = insertvalue { i64, i8* } undef, i64 %.unpack, 0
  %.unpack2 = load i8*, i8** @seq.var.1847.1, align 8
  %2 = insertvalue { i64, i8* } %1, i8* %.unpack2, 1
  %3 = call i8* @"dlsym.ptr[byte].str.str"({ i64, i8* } %2, { i64, i8* } { i64 11, i8* getelementptr inbounds ([12 x i8], [12 x i8]* @str_literal.47, i32 0, i32 0) })
  %4 = bitcast i8* %3 to i8* (i64)*
  %5 = call i8* %4(i64 %0)
  ret i8* %5
}

; Function Attrs: noinline uwtable
define i8* @"dlsym.ptr[byte].str.str"({ i64, i8* }, { i64, i8* }) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %2 = call { i64, i8* } @"str::__add__.str.str.str"({ i64, i8* } %0, { i64, i8* } { i64 1, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @str_literal.36, i32 0, i32 0) })
  %3 = call { i64, i8* } @"str::__add__.str.str.str"({ i64, i8* } %2, { i64, i8* } %1)
  %4 = load i8*, i8** @seq.var.1817, align 8
  %5 = call i8 @"dict[str,ptr[byte]]::__contains__.bool.dict[str,ptr[byte]].str"(i8* %4, { i64, i8* } %3)
  %6 = and i8 %5, 1
  %7 = icmp eq i8 %6, 0
  br i1 %7, label %11, label %8

; <label>:8:                                      ; preds = %preamble
  %9 = load i8*, i8** @seq.var.1817, align 8
  %10 = call i8* @"dict[str,ptr[byte]]::__getitem__.ptr[byte].dict[str,ptr[byte]].str"(i8* %9, { i64, i8* } %3)
  ret i8* %10

; <label>:11:                                     ; preds = %preamble
  %.b = load i1, i1* @seq.var.113, align 1
  %12 = select i1 %.b, i64 2, i64 0
  %.b48 = load i1, i1* @seq.var.114, align 1
  %13 = select i1 %.b48, i64 8, i64 0
  %14 = or i64 %13, %12
  %15 = call i8* @"dlopen.ptr[byte].str.int"({ i64, i8* } %0, i64 %14)
  %16 = call i8* @"_dlsym.ptr[byte].ptr[byte].str"(i8* %15, { i64, i8* } %1)
  %17 = load i8*, i8** @seq.var.1817, align 8
  call void @"dict[str,ptr[byte]]::__setitem__.void.dict[str,ptr[byte]].str.ptr[byte]"(i8* %17, { i64, i8* } %3, i8* %16)
  ret i8* %16
}

; Function Attrs: noinline uwtable
define i8 @"dict[str,ptr[byte]]::__contains__.bool.dict[str,ptr[byte]].str"(i8* nocapture readonly, { i64, i8* }) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %2 = call i64 @"dict[str,ptr[byte]]::_kh_get.int.dict[str,ptr[byte]].str"(i8* %0, { i64, i8* } %1)
  %3 = call i64 @"dict[str,ptr[byte]]::_kh_end.int.dict[str,ptr[byte]]"(i8* %0)
  %4 = icmp ne i64 %2, %3
  %5 = zext i1 %4 to i8
  ret i8 %5
}

; Function Attrs: noinline uwtable
define i64 @"dict[str,ptr[byte]]::_kh_get.int.dict[str,ptr[byte]].str"(i8* nocapture readonly, { i64, i8* }) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.elt = bitcast i8* %0 to i64*
  %.unpack = load i64, i64* %.elt, align 8
  %2 = icmp eq i64 %.unpack, 0
  br i1 %2, label %30, label %3

; <label>:3:                                      ; preds = %preamble
  %4 = add i64 %.unpack, -1
  %5 = call i64 @"_dict_hash[str].int.str"({ i64, i8* } %1)
  %6 = and i64 %5, %4
  %.elt51 = getelementptr inbounds i8, i8* %0, i64 32
  %7 = bitcast i8* %.elt51 to i32**
  %.unpack52134 = load i32*, i32** %7, align 8
  %8 = call i64 @"__ac_isempty.int.ptr[u32].int"(i32* %.unpack52134, i64 %6)
  %9 = icmp eq i64 %8, 0
  br i1 %9, label %.lr.ph, label %exit

.lr.ph:                                           ; preds = %3
  %.elt123 = getelementptr inbounds i8, i8* %0, i64 40
  %10 = bitcast i8* %.elt123 to { i64, i8* }**
  br label %13

while:                                            ; preds = %body
  %11 = call i64 @"__ac_isempty.int.ptr[u32].int"(i32* %.unpack52134, i64 %24)
  %12 = icmp eq i64 %11, 0
  br i1 %12, label %13, label %exit

; <label>:13:                                     ; preds = %while, %.lr.ph
  %.0136 = phi i64 [ 0, %.lr.ph ], [ %22, %while ]
  %.030135 = phi i64 [ %6, %.lr.ph ], [ %24, %while ]
  %14 = call i64 @"__ac_isdel.int.ptr[u32].int"(i32* %.unpack52134, i64 %.030135)
  %15 = icmp eq i64 %14, 0
  br i1 %15, label %16, label %body

; <label>:16:                                     ; preds = %13
  %.unpack124 = load { i64, i8* }*, { i64, i8* }** %10, align 8
  %.elt127 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack124, i64 %.030135, i32 0
  %.unpack128 = load i64, i64* %.elt127, align 8
  %17 = insertvalue { i64, i8* } undef, i64 %.unpack128, 0
  %.elt129 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack124, i64 %.030135, i32 1
  %.unpack130 = load i8*, i8** %.elt129, align 8
  %18 = insertvalue { i64, i8* } %17, i8* %.unpack130, 1
  %19 = call i8 @"str::__ne__.bool.str.str"({ i64, i8* } %18, { i64, i8* } %1)
  %20 = and i8 %19, 1
  %21 = icmp eq i8 %20, 0
  br i1 %21, label %exit, label %body

body:                                             ; preds = %16, %13
  %22 = add i64 %.0136, 1
  %23 = add i64 %22, %.030135
  %24 = and i64 %23, %4
  %25 = icmp eq i64 %24, %6
  br i1 %25, label %26, label %while

; <label>:26:                                     ; preds = %body
  %.unpack86 = load i64, i64* %.elt, align 8
  ret i64 %.unpack86

exit:                                             ; preds = %16, %while, %3
  %.030.lcssa = phi i64 [ %6, %3 ], [ %.030135, %16 ], [ %24, %while ]
  %27 = call i64 @"__ac_iseither.int.ptr[u32].int"(i32* %.unpack52134, i64 %.030.lcssa)
  %28 = icmp eq i64 %27, 0
  br i1 %28, label %30, label %29

; <label>:29:                                     ; preds = %exit
  %.unpack72 = load i64, i64* %.elt, align 8
  br label %30

; <label>:30:                                     ; preds = %29, %exit, %preamble
  %31 = phi i64 [ %.unpack72, %29 ], [ 0, %preamble ], [ %.030.lcssa, %exit ]
  ret i64 %31
}

; Function Attrs: noinline norecurse nounwind readonly uwtable
define i64 @"dict[str,ptr[byte]]::_kh_end.int.dict[str,ptr[byte]]"(i8* nocapture readonly) local_unnamed_addr #8 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.elt = bitcast i8* %0 to i64*
  %.unpack = load i64, i64* %.elt, align 8
  ret i64 %.unpack
}

; Function Attrs: noinline uwtable
define i8* @"dict[str,ptr[byte]]::__getitem__.ptr[byte].dict[str,ptr[byte]].str"(i8* nocapture readonly, { i64, i8* }) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %2 = call i64 @"dict[str,ptr[byte]]::_kh_get.int.dict[str,ptr[byte]].str"(i8* %0, { i64, i8* } %1)
  %3 = call i64 @"dict[str,ptr[byte]]::_kh_end.int.dict[str,ptr[byte]]"(i8* %0)
  %4 = icmp eq i64 %2, %3
  br i1 %4, label %9, label %5

; <label>:5:                                      ; preds = %preamble
  %.elt59 = getelementptr inbounds i8, i8* %0, i64 48
  %6 = bitcast i8* %.elt59 to i8***
  %.unpack60 = load i8**, i8*** %6, align 8
  %7 = getelementptr i8*, i8** %.unpack60, i64 %2
  %8 = load i8*, i8** %7, align 8
  ret i8* %8

; <label>:9:                                      ; preds = %preamble
  %10 = call { i64, i8* } @"str::__str__.str.str"({ i64, i8* } %1)
  %11 = call i8* @seq_alloc(i64 80)
  call void @"KeyError::__init__.void.KeyError.str"(i8* %11, { i64, i8* } %10)
  %.repack31.repack = getelementptr inbounds i8, i8* %11, i64 32
  %12 = bitcast i8* %.repack31.repack to i64*
  store i64 11, i64* %12, align 8
  %.repack31.repack41 = getelementptr inbounds i8, i8* %11, i64 40
  %13 = bitcast i8* %.repack31.repack41 to i8**
  store i8* getelementptr inbounds ([12 x i8], [12 x i8]* @str_literal.37, i64 0, i64 0), i8** %13, align 8
  %.repack33.repack = getelementptr inbounds i8, i8* %11, i64 48
  %14 = bitcast i8* %.repack33.repack to i64*
  store i64 73, i64* %14, align 8
  %.repack33.repack39 = getelementptr inbounds i8, i8* %11, i64 56
  %15 = bitcast i8* %.repack33.repack39 to i8**
  store i8* getelementptr inbounds ([74 x i8], [74 x i8]* @str_literal.38, i64 0, i64 0), i8** %15, align 8
  %.repack35 = getelementptr inbounds i8, i8* %11, i64 64
  %16 = bitcast i8* %.repack35 to i64*
  store i64 47, i64* %16, align 8
  %.repack37 = getelementptr inbounds i8, i8* %11, i64 72
  %17 = bitcast i8* %.repack37 to i64*
  store i64 9, i64* %17, align 8
  %18 = call i8* @seq_alloc_exc(i32 -71732691, i8* %11)
  call void @seq_throw(i8* %18)
  unreachable
}

; Function Attrs: noinline uwtable
define i8* @"dlopen.ptr[byte].str.int"({ i64, i8* }, i64) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %2 = load i8*, i8** @seq.var.1810, align 8
  %3 = call i8 @"dict[str,ptr[byte]]::__contains__.bool.dict[str,ptr[byte]].str"(i8* %2, { i64, i8* } %0)
  %4 = and i8 %3, 1
  %5 = icmp eq i8 %4, 0
  br i1 %5, label %9, label %6

; <label>:6:                                      ; preds = %preamble
  %7 = load i8*, i8** @seq.var.1810, align 8
  %8 = call i8* @"dict[str,ptr[byte]]::__getitem__.ptr[byte].dict[str,ptr[byte]].str"(i8* %7, { i64, i8* } %0)
  ret i8* %8

; <label>:9:                                      ; preds = %preamble
  %10 = call i8* @"str::c_str.ptr[byte].str"({ i64, i8* } %0)
  %11 = call i8* @dlopen(i8* %10, i64 %1)
  %12 = icmp eq i8* %11, null
  br i1 %12, label %13, label %23

; <label>:13:                                     ; preds = %9
  %14 = call { i64, i8* } @dlerror.str()
  %15 = call i8* @seq_alloc(i64 80)
  call void @"CError::__init__.void.CError.str"(i8* %15, { i64, i8* } %14)
  %.repack44.repack = getelementptr inbounds i8, i8* %15, i64 32
  %16 = bitcast i8* %.repack44.repack to i64*
  store i64 6, i64* %16, align 8
  %.repack44.repack54 = getelementptr inbounds i8, i8* %15, i64 40
  %17 = bitcast i8* %.repack44.repack54 to i8**
  store i8* getelementptr inbounds ([7 x i8], [7 x i8]* @str_literal.43, i64 0, i64 0), i8** %17, align 8
  %.repack46.repack = getelementptr inbounds i8, i8* %15, i64 48
  %18 = bitcast i8* %.repack46.repack to i64*
  store i64 63, i64* %18, align 8
  %.repack46.repack52 = getelementptr inbounds i8, i8* %15, i64 56
  %19 = bitcast i8* %.repack46.repack52 to i8**
  store i8* getelementptr inbounds ([64 x i8], [64 x i8]* @str_literal.44, i64 0, i64 0), i8** %19, align 8
  %.repack48 = getelementptr inbounds i8, i8* %15, i64 64
  %20 = bitcast i8* %.repack48 to i64*
  store i64 10, i64* %20, align 8
  %.repack50 = getelementptr inbounds i8, i8* %15, i64 72
  %21 = bitcast i8* %.repack50 to i64*
  store i64 9, i64* %21, align 8
  %22 = call i8* @seq_alloc_exc(i32 -1224222892, i8* %15)
  call void @seq_throw(i8* %22)
  unreachable

; <label>:23:                                     ; preds = %9
  %24 = load i8*, i8** @seq.var.1810, align 8
  call void @"dict[str,ptr[byte]]::__setitem__.void.dict[str,ptr[byte]].str.ptr[byte]"(i8* %24, { i64, i8* } %0, i8* nonnull %11)
  ret i8* %11
}

; Function Attrs: noinline uwtable
declare i8* @dlopen(i8*, i64) local_unnamed_addr #0

; Function Attrs: noinline nounwind uwtable
define noalias i8* @"str::c_str.ptr[byte].str"({ i64, i8* }) local_unnamed_addr #3 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %1 = call i64 @"len[str].int.str"({ i64, i8* } %0)
  %2 = icmp sgt i64 %1, -1
  br i1 %2, label %assert_pass, label %assert_fail

assert_fail:                                      ; preds = %preamble
  call void @seq_assert_failed({ i64, i8* } { i64 60, i8* getelementptr inbounds ([61 x i8], [61 x i8]* @str_literal.39, i32 0, i32 0) }, i64 120)
  unreachable

assert_pass:                                      ; preds = %preamble
  %.fca.1.extract = extractvalue { i64, i8* } %0, 1
  %3 = add i64 %1, 1
  %4 = call i8* @seq_alloc_atomic(i64 %3)
  call void @seq.memcpy(i8* %4, i8* %.fca.1.extract, i64 %1)
  %5 = getelementptr i8, i8* %4, i64 %1
  store i8 0, i8* %5, align 1
  ret i8* %4
}

; Function Attrs: noinline uwtable
define { i64, i8* } @dlerror.str() local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %0 = call i8* @dlerror()
  %1 = call { i64, i8* } @"str::from_ptr.str.ptr[byte]"(i8* %0)
  ret { i64, i8* } %1
}

; Function Attrs: noinline uwtable
declare i8* @dlerror() local_unnamed_addr #0

; Function Attrs: noinline norecurse nounwind uwtable
define void @"CError::__init__.void.CError.str"(i8* nocapture, { i64, i8* }) local_unnamed_addr #7 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.repack.repack = bitcast i8* %0 to i64*
  store i64 6, i64* %.repack.repack, align 8
  %.repack.repack19 = getelementptr inbounds i8, i8* %0, i64 8
  %2 = bitcast i8* %.repack.repack19 to i8**
  store i8* getelementptr inbounds ([7 x i8], [7 x i8]* @str_literal.40, i64 0, i64 0), i8** %2, align 8
  %.repack3.repack = getelementptr inbounds i8, i8* %0, i64 16
  %3 = bitcast i8* %.repack3.repack to i64*
  %.elt4.elt = extractvalue { i64, i8* } %1, 0
  store i64 %.elt4.elt, i64* %3, align 8
  %.repack3.repack17 = getelementptr inbounds i8, i8* %0, i64 24
  %4 = bitcast i8* %.repack3.repack17 to i8**
  %.elt4.elt18 = extractvalue { i64, i8* } %1, 1
  store i8* %.elt4.elt18, i8** %4, align 8
  %.repack5.repack = getelementptr inbounds i8, i8* %0, i64 32
  %5 = bitcast i8* %.repack5.repack to i64*
  store i64 0, i64* %5, align 8
  %.repack5.repack15 = getelementptr inbounds i8, i8* %0, i64 40
  %6 = bitcast i8* %.repack5.repack15 to i8**
  store i8* getelementptr inbounds ([1 x i8], [1 x i8]* @str_literal.41, i64 0, i64 0), i8** %6, align 8
  %.repack7.repack = getelementptr inbounds i8, i8* %0, i64 48
  %7 = bitcast i8* %.repack7.repack to i64*
  store i64 0, i64* %7, align 8
  %.repack7.repack13 = getelementptr inbounds i8, i8* %0, i64 56
  %8 = bitcast i8* %.repack7.repack13 to i8**
  store i8* getelementptr inbounds ([1 x i8], [1 x i8]* @str_literal.42, i64 0, i64 0), i8** %8, align 8
  %.repack9 = getelementptr inbounds i8, i8* %0, i64 64
  call void @llvm.memset.p0i8.i64(i8* nonnull %.repack9, i8 0, i64 16, i32 8, i1 false)
  ret void
}

; Function Attrs: noinline uwtable
define void @"dict[str,ptr[byte]]::__setitem__.void.dict[str,ptr[byte]].str.ptr[byte]"(i8* nocapture, { i64, i8* }, i8*) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %3 = call { i64, i64 } @"dict[str,ptr[byte]]::_kh_put.tuple[int,int].dict[str,ptr[byte]].str"(i8* %0, { i64, i8* } %1)
  %.fca.1.extract = extractvalue { i64, i64 } %3, 1
  %.elt30 = getelementptr inbounds i8, i8* %0, i64 48
  %4 = bitcast i8* %.elt30 to i8***
  %.unpack31 = load i8**, i8*** %4, align 8
  %5 = getelementptr i8*, i8** %.unpack31, i64 %.fca.1.extract
  store i8* %2, i8** %5, align 8
  ret void
}

; Function Attrs: noinline uwtable
define { i64, i64 } @"dict[str,ptr[byte]]::_kh_put.tuple[int,int].dict[str,ptr[byte]].str"(i8* nocapture, { i64, i8* }) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.elt = bitcast i8* %0 to i64*
  %.unpack = load i64, i64* %.elt, align 8
  %.elt88 = getelementptr inbounds i8, i8* %0, i64 16
  %2 = bitcast i8* %.elt88 to i64*
  %.unpack89 = load i64, i64* %2, align 8
  %.elt90 = getelementptr inbounds i8, i8* %0, i64 24
  %3 = bitcast i8* %.elt90 to i64*
  %.unpack91 = load i64, i64* %3, align 8
  %4 = icmp slt i64 %.unpack89, %.unpack91
  br i1 %4, label %13, label %5

; <label>:5:                                      ; preds = %preamble
  %.elt86 = getelementptr inbounds i8, i8* %0, i64 8
  %6 = bitcast i8* %.elt86 to i64*
  %.unpack87 = load i64, i64* %6, align 8
  %7 = shl i64 %.unpack87, 1
  %8 = icmp sgt i64 %.unpack, %7
  br i1 %8, label %9, label %11

; <label>:9:                                      ; preds = %5
  %10 = add i64 %.unpack, -1
  call void @"dict[str,ptr[byte]]::_kh_resize.void.dict[str,ptr[byte]].int"(i8* nonnull %0, i64 %10)
  br label %13

; <label>:11:                                     ; preds = %5
  %12 = add i64 %.unpack, 1
  call void @"dict[str,ptr[byte]]::_kh_resize.void.dict[str,ptr[byte]].int"(i8* nonnull %0, i64 %12)
  br label %13

; <label>:13:                                     ; preds = %11, %9, %preamble
  %.unpack99 = load i64, i64* %.elt, align 8
  %14 = add i64 %.unpack99, -1
  %15 = call i64 @"_dict_hash[str].int.str"({ i64, i8* } %1)
  %16 = and i64 %15, %14
  %.elt120 = getelementptr inbounds i8, i8* %0, i64 32
  %17 = bitcast i8* %.elt120 to i32**
  %.unpack121 = load i32*, i32** %17, align 8
  %18 = call i64 @"__ac_isempty.int.ptr[u32].int"(i32* %.unpack121, i64 %16)
  %19 = icmp eq i64 %18, 0
  br i1 %19, label %.lr.ph, label %.thread368

.lr.ph:                                           ; preds = %13
  %.elt359 = getelementptr inbounds i8, i8* %0, i64 40
  %20 = bitcast i8* %.elt359 to { i64, i8* }**
  br label %23

while:                                            ; preds = %body
  %21 = call i64 @"__ac_isempty.int.ptr[u32].int"(i32* %.unpack121, i64 %34)
  %22 = icmp eq i64 %21, 0
  br i1 %22, label %23, label %exit

; <label>:23:                                     ; preds = %while, %.lr.ph
  %.080372 = phi i64 [ 0, %.lr.ph ], [ %32, %while ]
  %.081371 = phi i64 [ %16, %.lr.ph ], [ %34, %while ]
  %.082370 = phi i64 [ %.unpack99, %.lr.ph ], [ %spec.select, %while ]
  %24 = call i64 @"__ac_isdel.int.ptr[u32].int"(i32* %.unpack121, i64 %.081371)
  %25 = icmp eq i64 %24, 0
  br i1 %25, label %26, label %body

; <label>:26:                                     ; preds = %23
  %.unpack360 = load { i64, i8* }*, { i64, i8* }** %20, align 8
  %.elt363 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack360, i64 %.081371, i32 0
  %.unpack364 = load i64, i64* %.elt363, align 8
  %27 = insertvalue { i64, i8* } undef, i64 %.unpack364, 0
  %.elt365 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack360, i64 %.081371, i32 1
  %.unpack366 = load i8*, i8** %.elt365, align 8
  %28 = insertvalue { i64, i8* } %27, i8* %.unpack366, 1
  %29 = call i8 @"str::__ne__.bool.str.str"({ i64, i8* } %28, { i64, i8* } %1)
  %30 = and i8 %29, 1
  %31 = icmp eq i8 %30, 0
  br i1 %31, label %exit, label %body

body:                                             ; preds = %26, %23
  %spec.select = select i1 %25, i64 %.082370, i64 %.081371
  %32 = add i64 %.080372, 1
  %33 = add i64 %32, %.081371
  %34 = and i64 %33, %14
  %35 = icmp eq i64 %34, %16
  br i1 %35, label %exit, label %while

exit:                                             ; preds = %body, %26, %while
  %.084.ph = phi i64 [ %.unpack99, %26 ], [ %spec.select, %body ], [ %.unpack99, %while ]
  %.2.ph = phi i64 [ %.082370, %26 ], [ %spec.select, %body ], [ %spec.select, %while ]
  %.1.ph = phi i64 [ %.081371, %26 ], [ %16, %body ], [ %34, %while ]
  %.unpack141 = load i64, i64* %.elt, align 8
  %36 = icmp eq i64 %.084.ph, %.unpack141
  br i1 %36, label %37, label %.thread368

; <label>:37:                                     ; preds = %exit
  %38 = call i64 @"__ac_isempty.int.ptr[u32].int"(i32* %.unpack121, i64 %.1.ph)
  %39 = icmp ne i64 %38, 0
  %40 = icmp ne i64 %.2.ph, %.084.ph
  %or.cond = and i1 %40, %39
  %spec.select369 = select i1 %or.cond, i64 %.2.ph, i64 %.1.ph
  br label %.thread368

.thread368:                                       ; preds = %37, %exit, %13
  %.185 = phi i64 [ %.084.ph, %exit ], [ %16, %13 ], [ %spec.select369, %37 ]
  %41 = call i64 @"__ac_isempty.int.ptr[u32].int"(i32* %.unpack121, i64 %.185)
  %42 = icmp eq i64 %41, 0
  br i1 %42, label %48, label %43

; <label>:43:                                     ; preds = %.thread368
  %.elt254 = getelementptr inbounds i8, i8* %0, i64 40
  %44 = bitcast i8* %.elt254 to { i64, i8* }**
  %.unpack255 = load { i64, i8* }*, { i64, i8* }** %44, align 8
  %.repack258 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack255, i64 %.185, i32 0
  %.elt259 = extractvalue { i64, i8* } %1, 0
  store i64 %.elt259, i64* %.repack258, align 8
  %.repack260 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack255, i64 %.185, i32 1
  %.elt261 = extractvalue { i64, i8* } %1, 1
  store i8* %.elt261, i8** %.repack260, align 8
  %.unpack271 = load i32*, i32** %17, align 8
  call void @"__ac_set_isboth_false.void.ptr[u32].int"(i32* %.unpack271, i64 %.185)
  %.elt278 = getelementptr inbounds i8, i8* %0, i64 8
  %45 = bitcast i8* %.elt278 to i64*
  %.unpack279 = load i64, i64* %45, align 8
  %.unpack281 = load i64, i64* %2, align 8
  %46 = add i64 %.unpack279, 1
  %47 = add i64 %.unpack281, 1
  store i64 %46, i64* %45, align 8
  store i64 %47, i64* %2, align 8
  br label %55

; <label>:48:                                     ; preds = %.thread368
  %49 = call i64 @"__ac_isdel.int.ptr[u32].int"(i32* %.unpack121, i64 %.185)
  %50 = icmp eq i64 %49, 0
  br i1 %50, label %55, label %51

; <label>:51:                                     ; preds = %48
  %.elt192 = getelementptr inbounds i8, i8* %0, i64 40
  %52 = bitcast i8* %.elt192 to { i64, i8* }**
  %.unpack193 = load { i64, i8* }*, { i64, i8* }** %52, align 8
  %.repack = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack193, i64 %.185, i32 0
  %.elt196 = extractvalue { i64, i8* } %1, 0
  store i64 %.elt196, i64* %.repack, align 8
  %.repack197 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack193, i64 %.185, i32 1
  %.elt198 = extractvalue { i64, i8* } %1, 1
  store i8* %.elt198, i8** %.repack197, align 8
  %.unpack208 = load i32*, i32** %17, align 8
  call void @"__ac_set_isboth_false.void.ptr[u32].int"(i32* %.unpack208, i64 %.185)
  %.elt215 = getelementptr inbounds i8, i8* %0, i64 8
  %53 = bitcast i8* %.elt215 to i64*
  %.unpack216 = load i64, i64* %53, align 8
  %54 = add i64 %.unpack216, 1
  store i64 %54, i64* %53, align 8
  br label %55

; <label>:55:                                     ; preds = %51, %48, %43
  %.0 = phi i64 [ 1, %43 ], [ 2, %51 ], [ 0, %48 ]
  %56 = insertvalue { i64, i64 } undef, i64 %.0, 0
  %57 = insertvalue { i64, i64 } %56, i64 %.185, 1
  ret { i64, i64 } %57
}

; Function Attrs: noinline uwtable
define void @"dict[str,ptr[byte]]::_kh_resize.void.dict[str,ptr[byte]].int"(i8* nocapture, i64) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %2 = add i64 %1, -1
  %3 = ashr i64 %2, 1
  %4 = or i64 %3, %2
  %5 = ashr i64 %4, 2
  %6 = or i64 %5, %4
  %7 = ashr i64 %6, 4
  %8 = or i64 %7, %6
  %9 = ashr i64 %8, 8
  %10 = or i64 %9, %8
  %11 = ashr i64 %10, 16
  %12 = or i64 %11, %10
  %13 = ashr i64 %12, 32
  %14 = or i64 %13, %12
  %15 = add i64 %14, 1
  %16 = icmp sgt i64 %15, 4
  %spec.store.select = select i1 %16, i64 %15, i64 4
  %.elt151 = getelementptr inbounds i8, i8* %0, i64 8
  %17 = bitcast i8* %.elt151 to i64*
  %.unpack152 = load i64, i64* %17, align 8
  %18 = sitofp i64 %spec.store.select to double
  %19 = fmul double %18, 7.700000e-01
  %20 = fadd double %19, 5.000000e-01
  %21 = fptosi double %20 to i64
  %22 = icmp slt i64 %.unpack152, %21
  br i1 %22, label %23, label %74

; <label>:23:                                     ; preds = %preamble
  %24 = call i64 @"__ac_fsize[int].int.int"(i64 %spec.store.select)
  %25 = shl i64 %24, 2
  %26 = call i8* @seq_alloc_atomic(i64 %25)
  %27 = bitcast i8* %26 to i32*
  %28 = icmp sgt i64 %24, 0
  br i1 %28, label %body.lr.ph, label %exit

body.lr.ph:                                       ; preds = %23
  call void @llvm.memset.p0i8.i64(i8* %26, i8 -86, i64 %25, i32 4, i1 false)
  br label %exit

exit:                                             ; preds = %body.lr.ph, %23
  %.elt = bitcast i8* %0 to i64*
  %.unpack = load i64, i64* %.elt, align 8
  %29 = icmp slt i64 %.unpack, %spec.store.select
  %.elt171 = getelementptr inbounds i8, i8* %0, i64 40
  %30 = bitcast i8* %.elt171 to i8**
  br i1 %29, label %31, label %exit._crit_edge

; <label>:31:                                     ; preds = %exit
  %.unpack172390 = load i8*, i8** %30, align 8
  %32 = shl i64 %spec.store.select, 4
  %33 = call i8* @"realloc.ptr[byte].ptr[byte].int"(i8* %.unpack172390, i64 %32)
  %.elt403 = getelementptr inbounds i8, i8* %0, i64 48
  %34 = bitcast i8* %.elt403 to i8**
  %.unpack404454 = load i8*, i8** %34, align 8
  store i8* %33, i8** %30, align 8
  %35 = shl i64 %spec.store.select, 3
  %36 = call i8* @"realloc.ptr[byte].ptr[byte].int"(i8* %.unpack404454, i64 %35)
  store i8* %36, i8** %34, align 8
  %.unpack176466.pre = load i64, i64* %.elt, align 8
  br label %exit._crit_edge

exit._crit_edge:                                  ; preds = %31, %exit
  %.unpack176466 = phi i64 [ %.unpack176466.pre, %31 ], [ %.unpack, %exit ]
  %37 = icmp eq i64 %.unpack176466, 0
  %.pre = getelementptr inbounds i8, i8* %0, i64 32
  br i1 %37, label %exit9.thread, label %body2.lr.ph

body2.lr.ph:                                      ; preds = %exit._crit_edge
  %38 = bitcast i8* %.pre to i32**
  %39 = bitcast i8* %.elt171 to { i64, i8* }**
  %.elt290 = getelementptr inbounds i8, i8* %0, i64 48
  %40 = bitcast i8* %.elt290 to i8***
  %41 = add nsw i64 %spec.store.select, -1
  br label %body2

body2:                                            ; preds = %while1, %body2.lr.ph
  %.unpack176485 = phi i64 [ %.unpack176466, %body2.lr.ph ], [ %.unpack176, %while1 ]
  %.1468 = phi i64 [ 0, %body2.lr.ph ], [ %62, %while1 ]
  %.unpack184 = load i32*, i32** %38, align 8
  %42 = call i64 @"__ac_iseither.int.ptr[u32].int"(i32* %.unpack184, i64 %.1468)
  %43 = icmp eq i64 %42, 0
  br i1 %43, label %44, label %while1

; <label>:44:                                     ; preds = %body2
  %.unpack289 = load { i64, i8* }*, { i64, i8* }** %39, align 8
  %.unpack291 = load i8**, i8*** %40, align 8
  %.elt292 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack289, i64 %.1468, i32 0
  %.unpack293 = load i64, i64* %.elt292, align 8
  %.elt294 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack289, i64 %.1468, i32 1
  %.unpack295 = load i8*, i8** %.elt294, align 8
  %45 = getelementptr i8*, i8** %.unpack291, i64 %.1468
  %46 = load i8*, i8** %45, align 8
  call void @"__ac_set_isdel_true.void.ptr[u32].int"(i32* %.unpack184, i64 %.1468)
  br label %while3

while3:                                           ; preds = %58, %44
  %.0149 = phi i8* [ %46, %44 ], [ %60, %58 ]
  %.sroa.071.0 = phi i64 [ %.unpack293, %44 ], [ %.unpack356, %58 ]
  %.sroa.4.0 = phi i8* [ %.unpack295, %44 ], [ %.unpack358, %58 ]
  %.fca.0.insert58 = insertvalue { i64, i8* } undef, i64 %.sroa.071.0, 0
  %.fca.1.insert60 = insertvalue { i64, i8* } %.fca.0.insert58, i8* %.sroa.4.0, 1
  %47 = call i64 @"_dict_hash[str].int.str"({ i64, i8* } %.fca.1.insert60)
  %.0148463 = and i64 %47, %41
  %48 = call i64 @"__ac_isempty.int.ptr[u32].int"(i32* %27, i64 %.0148463)
  %49 = icmp eq i64 %48, 0
  br i1 %49, label %body6.preheader, label %exit7

body6.preheader:                                  ; preds = %while3
  br label %body6

body6:                                            ; preds = %body6, %body6.preheader
  %.0148465 = phi i64 [ %.0148, %body6 ], [ %.0148463, %body6.preheader ]
  %.0150464 = phi i64 [ %50, %body6 ], [ 0, %body6.preheader ]
  %50 = add i64 %.0150464, 1
  %51 = add i64 %50, %.0148465
  %.0148 = and i64 %51, %41
  %52 = call i64 @"__ac_isempty.int.ptr[u32].int"(i32* %27, i64 %.0148)
  %53 = icmp eq i64 %52, 0
  br i1 %53, label %body6, label %exit7

exit7:                                            ; preds = %body6, %while3
  %.0148.lcssa = phi i64 [ %.0148463, %while3 ], [ %.0148, %body6 ]
  call void @"__ac_set_isempty_false.void.ptr[u32].int"(i32* %27, i64 %.0148.lcssa)
  %.unpack297 = load i64, i64* %.elt, align 8
  %54 = icmp slt i64 %.0148.lcssa, %.unpack297
  br i1 %54, label %55, label %.thread

.thread:                                          ; preds = %exit7
  %.unpack352451 = load { i64, i8* }*, { i64, i8* }** %39, align 8
  br label %exit8

; <label>:55:                                     ; preds = %exit7
  %.unpack305 = load i32*, i32** %38, align 8
  %56 = call i64 @"__ac_iseither.int.ptr[u32].int"(i32* %.unpack305, i64 %.0148.lcssa)
  %57 = icmp eq i64 %56, 0
  %.unpack352 = load { i64, i8* }*, { i64, i8* }** %39, align 8
  br i1 %57, label %58, label %exit8

; <label>:58:                                     ; preds = %55
  %.elt355 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack352, i64 %.0148.lcssa, i32 0
  %.unpack356 = load i64, i64* %.elt355, align 8
  %.elt357 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack352, i64 %.0148.lcssa, i32 1
  %.unpack358 = load i8*, i8** %.elt357, align 8
  store i64 %.sroa.071.0, i64* %.elt355, align 8
  store i8* %.sroa.4.0, i8** %.elt357, align 8
  %.unpack375 = load i8**, i8*** %40, align 8
  %59 = getelementptr i8*, i8** %.unpack375, i64 %.0148.lcssa
  %60 = load i8*, i8** %59, align 8
  store i8* %.0149, i8** %59, align 8
  %.unpack385 = load i32*, i32** %38, align 8
  call void @"__ac_set_isdel_true.void.ptr[u32].int"(i32* %.unpack385, i64 %.0148.lcssa)
  br label %while3

exit8:                                            ; preds = %55, %.thread
  %.unpack352452 = phi { i64, i8* }* [ %.unpack352451, %.thread ], [ %.unpack352, %55 ]
  %.repack324 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack352452, i64 %.0148.lcssa, i32 0
  store i64 %.sroa.071.0, i64* %.repack324, align 8
  %.repack325 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %.unpack352452, i64 %.0148.lcssa, i32 1
  store i8* %.sroa.4.0, i8** %.repack325, align 8
  %.unpack340 = load i8**, i8*** %40, align 8
  %61 = getelementptr i8*, i8** %.unpack340, i64 %.0148.lcssa
  store i8* %.0149, i8** %61, align 8
  %.unpack176.pre = load i64, i64* %.elt, align 8
  br label %while1

while1:                                           ; preds = %exit8, %body2
  %.unpack176 = phi i64 [ %.unpack176485, %body2 ], [ %.unpack176.pre, %exit8 ]
  %62 = add i64 %.1468, 1
  %63 = icmp eq i64 %62, %.unpack176
  br i1 %63, label %exit9, label %body2

exit9:                                            ; preds = %while1
  %64 = icmp sgt i64 %.unpack176, %spec.store.select
  br i1 %64, label %65, label %exit9.thread

; <label>:65:                                     ; preds = %exit9
  %.unpack186277.lcssa = load i8*, i8** %30, align 8
  %66 = shl i64 %spec.store.select, 4
  %67 = call i8* @"realloc.ptr[byte].ptr[byte].int"(i8* %.unpack186277.lcssa, i64 %66)
  %68 = bitcast i8* %.elt290 to i8**
  %.unpack231453 = load i8*, i8** %68, align 8
  store i8* %67, i8** %30, align 8
  %69 = shl i64 %spec.store.select, 3
  %70 = call i8* @"realloc.ptr[byte].ptr[byte].int"(i8* %.unpack231453, i64 %69)
  store i8* %70, i8** %68, align 8
  br label %exit9.thread

exit9.thread:                                     ; preds = %65, %exit9, %exit._crit_edge
  %.unpack192 = load i64, i64* %17, align 8
  store i64 %spec.store.select, i64* %.elt, align 8
  %.repack206 = getelementptr inbounds i8, i8* %0, i64 16
  %71 = bitcast i8* %.repack206 to i64*
  store i64 %.unpack192, i64* %71, align 8
  %.repack208 = getelementptr inbounds i8, i8* %0, i64 24
  %72 = bitcast i8* %.repack208 to i64*
  store i64 %21, i64* %72, align 8
  %73 = bitcast i8* %.pre to i8**
  store i8* %26, i8** %73, align 8
  br label %74

; <label>:74:                                     ; preds = %exit9.thread, %preamble
  ret void
}

; Function Attrs: noinline norecurse nounwind readnone uwtable
define i64 @"sizeof[ptr[byte]].int"() local_unnamed_addr #2 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  ret i64 8
}

; Function Attrs: noinline norecurse nounwind readnone uwtable
define i64 @seq.magic.__elemsize__.4() local_unnamed_addr #2 {
entry:
  ret i64 8
}

; Function Attrs: noinline uwtable
define i8* @"_dlsym.ptr[byte].ptr[byte].str"(i8*, { i64, i8* }) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %2 = call i8* @"str::c_str.ptr[byte].str"({ i64, i8* } %1)
  %3 = call i8* @dlsym(i8* %0, i8* %2)
  %4 = icmp eq i8* %3, null
  br i1 %4, label %5, label %15

; <label>:5:                                      ; preds = %preamble
  %6 = call { i64, i8* } @dlerror.str()
  %7 = call i8* @seq_alloc(i64 80)
  call void @"CError::__init__.void.CError.str"(i8* %7, { i64, i8* } %6)
  %.repack23.repack = getelementptr inbounds i8, i8* %7, i64 32
  %8 = bitcast i8* %.repack23.repack to i64*
  store i64 6, i64* %8, align 8
  %.repack23.repack33 = getelementptr inbounds i8, i8* %7, i64 40
  %9 = bitcast i8* %.repack23.repack33 to i8**
  store i8* getelementptr inbounds ([7 x i8], [7 x i8]* @str_literal.45, i64 0, i64 0), i8** %9, align 8
  %.repack25.repack = getelementptr inbounds i8, i8* %7, i64 48
  %10 = bitcast i8* %.repack25.repack to i64*
  store i64 63, i64* %10, align 8
  %.repack25.repack31 = getelementptr inbounds i8, i8* %7, i64 56
  %11 = bitcast i8* %.repack25.repack31 to i8**
  store i8* getelementptr inbounds ([64 x i8], [64 x i8]* @str_literal.46, i64 0, i64 0), i8** %11, align 8
  %.repack27 = getelementptr inbounds i8, i8* %7, i64 64
  %12 = bitcast i8* %.repack27 to i64*
  store i64 17, i64* %12, align 8
  %.repack29 = getelementptr inbounds i8, i8* %7, i64 72
  %13 = bitcast i8* %.repack29 to i64*
  store i64 9, i64* %13, align 8
  %14 = call i8* @seq_alloc_exc(i32 -1224222892, i8* %7)
  call void @seq_throw(i8* %14)
  unreachable

; <label>:15:                                     ; preds = %preamble
  ret i8* %3
}

; Function Attrs: noinline uwtable
declare i8* @dlsym(i8*, i8*) local_unnamed_addr #0

; Function Attrs: noinline uwtable
define void @"pyobj::exc_check.void"() local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %0 = alloca i8*, align 8
  %1 = alloca i8*, align 8
  %2 = alloca i8*, align 8
  store i8* null, i8** %0, align 8
  store i8* null, i8** %1, align 8
  store i8* null, i8** %2, align 8
  call void @"PyErr_Fetch.void.ptr[ptr[byte]].ptr[ptr[byte]].ptr[ptr[byte]]"(i8** nonnull %0, i8** nonnull %1, i8** nonnull %2)
  %3 = load i8*, i8** %0, align 8
  %4 = icmp eq i8* %3, null
  br i1 %4, label %33, label %5

; <label>:5:                                      ; preds = %preamble
  %6 = load i8*, i8** %1, align 8
  %7 = icmp eq i8* %6, null
  br i1 %7, label %10, label %8

; <label>:8:                                      ; preds = %5
  %9 = call i8* @"PyObject_Str.ptr[byte].ptr[byte]"(i8* nonnull %6)
  br label %10

; <label>:10:                                     ; preds = %8, %5
  %11 = phi i8* [ %9, %8 ], [ null, %5 ]
  %12 = insertvalue { i8* } zeroinitializer, i8* %11, 0
  %13 = call { i64, i8* } @"pyobj::to_str.str.pyobj.str.str"({ i8* } %12, { i64, i8* } { i64 6, i8* getelementptr inbounds ([7 x i8], [7 x i8]* @str_literal.54, i32 0, i32 0) }, { i64, i8* } { i64 22, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @str_literal.55, i32 0, i32 0) })
  %14 = load i8*, i8** %0, align 8
  %15 = call i8* @"str::c_str.ptr[byte].str"({ i64, i8* } { i64 8, i8* getelementptr inbounds ([9 x i8], [9 x i8]* @str_literal.57, i32 0, i32 0) })
  %16 = call i8* @"PyObject_GetAttrString.ptr[byte].ptr[byte].ptr[byte]"(i8* %14, i8* %15)
  %17 = insertvalue { i8* } zeroinitializer, i8* %16, 0
  %18 = call { i64, i8* } @"pyobj::to_str.str.pyobj.str.str"({ i8* } %17, { i64, i8* } { i64 6, i8* getelementptr inbounds ([7 x i8], [7 x i8]* @str_literal.58, i32 0, i32 0) }, { i64, i8* } { i64 0, i8* getelementptr inbounds ([1 x i8], [1 x i8]* @str_literal.59, i32 0, i32 0) })
  %19 = load i8*, i8** %0, align 8
  %20 = insertvalue { i8* } zeroinitializer, i8* %19, 0
  call void @"pyobj::decref.void.pyobj"({ i8* } %20)
  %21 = load i8*, i8** %1, align 8
  %22 = insertvalue { i8* } zeroinitializer, i8* %21, 0
  call void @"pyobj::decref.void.pyobj"({ i8* } %22)
  %23 = load i8*, i8** %2, align 8
  %24 = insertvalue { i8* } zeroinitializer, i8* %23, 0
  call void @"pyobj::decref.void.pyobj"({ i8* } %24)
  call void @"pyobj::decref.void.pyobj"({ i8* } %12)
  %25 = call i8* @seq_alloc(i64 96)
  call void @"PyError::__init__.void.PyError.str.str"(i8* %25, { i64, i8* } %13, { i64, i8* } %18)
  %.repack54.repack = getelementptr inbounds i8, i8* %25, i64 32
  %26 = bitcast i8* %.repack54.repack to i64*
  store i64 9, i64* %26, align 8
  %.repack54.repack64 = getelementptr inbounds i8, i8* %25, i64 40
  %27 = bitcast i8* %.repack54.repack64 to i8**
  store i8* getelementptr inbounds ([10 x i8], [10 x i8]* @str_literal.63, i64 0, i64 0), i8** %27, align 8
  %.repack56.repack = getelementptr inbounds i8, i8* %25, i64 48
  %28 = bitcast i8* %.repack56.repack to i64*
  store i64 63, i64* %28, align 8
  %.repack56.repack62 = getelementptr inbounds i8, i8* %25, i64 56
  %29 = bitcast i8* %.repack56.repack62 to i8**
  store i8* getelementptr inbounds ([64 x i8], [64 x i8]* @str_literal.64, i64 0, i64 0), i8** %29, align 8
  %.repack58 = getelementptr inbounds i8, i8* %25, i64 64
  %30 = bitcast i8* %.repack58 to i64*
  store i64 70, i64* %30, align 8
  %.repack60 = getelementptr inbounds i8, i8* %25, i64 72
  %31 = bitcast i8* %.repack60 to i64*
  store i64 13, i64* %31, align 8
  %32 = call i8* @seq_alloc_exc(i32 -1180362110, i8* %25)
  call void @seq_throw(i8* %32)
  unreachable

; <label>:33:                                     ; preds = %preamble
  ret void
}

; Function Attrs: noinline uwtable
define void @"PyErr_Fetch.void.ptr[ptr[byte]].ptr[ptr[byte]].ptr[ptr[byte]]"(i8**, i8**, i8**) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.unpack = load i64, i64* @seq.var.1847.0, align 8
  %3 = insertvalue { i64, i8* } undef, i64 %.unpack, 0
  %.unpack5 = load i8*, i8** @seq.var.1847.1, align 8
  %4 = insertvalue { i64, i8* } %3, i8* %.unpack5, 1
  %5 = call i8* @"dlsym.ptr[byte].str.str"({ i64, i8* } %4, { i64, i8* } { i64 11, i8* getelementptr inbounds ([12 x i8], [12 x i8]* @str_literal.48, i32 0, i32 0) })
  %6 = bitcast i8* %5 to void (i8**, i8**, i8**)*
  call void %6(i8** %0, i8** %1, i8** %2)
  ret void
}

; Function Attrs: noinline uwtable
define i8* @"PyObject_Str.ptr[byte].ptr[byte]"(i8*) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.unpack = load i64, i64* @seq.var.1847.0, align 8
  %1 = insertvalue { i64, i8* } undef, i64 %.unpack, 0
  %.unpack3 = load i8*, i8** @seq.var.1847.1, align 8
  %2 = insertvalue { i64, i8* } %1, i8* %.unpack3, 1
  %3 = call i8* @"dlsym.ptr[byte].str.str"({ i64, i8* } %2, { i64, i8* } { i64 12, i8* getelementptr inbounds ([13 x i8], [13 x i8]* @str_literal.49, i32 0, i32 0) })
  %4 = bitcast i8* %3 to i8* (i8*)*
  %5 = call i8* %4(i8* %0)
  ret i8* %5
}

; Function Attrs: noinline uwtable
define { i64, i8* } @"pyobj::to_str.str.pyobj.str.str"({ i8* }, { i64, i8* }, { i64, i8* }) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.fca.0.extract14 = extractvalue { i8* } %0, 0
  %3 = call i8* @"str::c_str.ptr[byte].str"({ i64, i8* } { i64 5, i8* getelementptr inbounds ([6 x i8], [6 x i8]* @str_literal.51, i32 0, i32 0) })
  %4 = call i8* @"str::c_str.ptr[byte].str"({ i64, i8* } %1)
  %5 = call i8* @"PyUnicode_AsEncodedString.ptr[byte].ptr[byte].ptr[byte].ptr[byte]"(i8* %.fca.0.extract14, i8* %3, i8* %4)
  %6 = icmp eq i8* %5, null
  br i1 %6, label %7, label %8

; <label>:7:                                      ; preds = %preamble
  ret { i64, i8* } %2

; <label>:8:                                      ; preds = %preamble
  %9 = call i8* @"PyBytes_AsString.ptr[byte].ptr[byte]"(i8* nonnull %5)
  %10 = insertvalue { i8* } zeroinitializer, i8* %5, 0
  call void @"pyobj::decref.void.pyobj"({ i8* } %10)
  %11 = call { i64, i8* } @"str::from_ptr.str.ptr[byte]"(i8* %9)
  ret { i64, i8* } %11
}

; Function Attrs: noinline uwtable
define i8* @"PyUnicode_AsEncodedString.ptr[byte].ptr[byte].ptr[byte].ptr[byte]"(i8*, i8*, i8*) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.unpack = load i64, i64* @seq.var.1847.0, align 8
  %3 = insertvalue { i64, i8* } undef, i64 %.unpack, 0
  %.unpack5 = load i8*, i8** @seq.var.1847.1, align 8
  %4 = insertvalue { i64, i8* } %3, i8* %.unpack5, 1
  %5 = call i8* @"dlsym.ptr[byte].str.str"({ i64, i8* } %4, { i64, i8* } { i64 25, i8* getelementptr inbounds ([26 x i8], [26 x i8]* @str_literal.50, i32 0, i32 0) })
  %6 = bitcast i8* %5 to i8* (i8*, i8*, i8*)*
  %7 = call i8* %6(i8* %0, i8* %1, i8* %2)
  ret i8* %7
}

; Function Attrs: noinline uwtable
define i8* @"PyBytes_AsString.ptr[byte].ptr[byte]"(i8*) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.unpack = load i64, i64* @seq.var.1847.0, align 8
  %1 = insertvalue { i64, i8* } undef, i64 %.unpack, 0
  %.unpack3 = load i8*, i8** @seq.var.1847.1, align 8
  %2 = insertvalue { i64, i8* } %1, i8* %.unpack3, 1
  %3 = call i8* @"dlsym.ptr[byte].str.str"({ i64, i8* } %2, { i64, i8* } { i64 16, i8* getelementptr inbounds ([17 x i8], [17 x i8]* @str_literal.52, i32 0, i32 0) })
  %4 = bitcast i8* %3 to i8* (i8*)*
  %5 = call i8* %4(i8* %0)
  ret i8* %5
}

; Function Attrs: noinline uwtable
define void @"pyobj::decref.void.pyobj"({ i8* }) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.fca.0.extract = extractvalue { i8* } %0, 0
  call void @"Py_DecRef.void.ptr[byte]"(i8* %.fca.0.extract)
  ret void
}

; Function Attrs: noinline uwtable
define void @"Py_DecRef.void.ptr[byte]"(i8*) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.unpack = load i64, i64* @seq.var.1847.0, align 8
  %1 = insertvalue { i64, i8* } undef, i64 %.unpack, 0
  %.unpack3 = load i8*, i8** @seq.var.1847.1, align 8
  %2 = insertvalue { i64, i8* } %1, i8* %.unpack3, 1
  %3 = call i8* @"dlsym.ptr[byte].str.str"({ i64, i8* } %2, { i64, i8* } { i64 9, i8* getelementptr inbounds ([10 x i8], [10 x i8]* @str_literal.53, i32 0, i32 0) })
  %4 = bitcast i8* %3 to void (i8*)*
  call void %4(i8* %0)
  ret void
}

; Function Attrs: noinline uwtable
define i8* @"PyObject_GetAttrString.ptr[byte].ptr[byte].ptr[byte]"(i8*, i8*) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.unpack = load i64, i64* @seq.var.1847.0, align 8
  %2 = insertvalue { i64, i8* } undef, i64 %.unpack, 0
  %.unpack4 = load i8*, i8** @seq.var.1847.1, align 8
  %3 = insertvalue { i64, i8* } %2, i8* %.unpack4, 1
  %4 = call i8* @"dlsym.ptr[byte].str.str"({ i64, i8* } %3, { i64, i8* } { i64 22, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @str_literal.56, i32 0, i32 0) })
  %5 = bitcast i8* %4 to i8* (i8*, i8*)*
  %6 = call i8* %5(i8* %0, i8* %1)
  ret i8* %6
}

; Function Attrs: noinline norecurse nounwind uwtable
define void @"PyError::__init__.void.PyError.str.str"(i8* nocapture, { i64, i8* }, { i64, i8* }) local_unnamed_addr #7 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.repack.repack.repack = bitcast i8* %0 to i64*
  store i64 7, i64* %.repack.repack.repack, align 8
  %.repack.repack.repack34 = getelementptr inbounds i8, i8* %0, i64 8
  %3 = bitcast i8* %.repack.repack.repack34 to i8**
  store i8* getelementptr inbounds ([8 x i8], [8 x i8]* @str_literal.60, i64 0, i64 0), i8** %3, align 8
  %.repack.repack18.repack = getelementptr inbounds i8, i8* %0, i64 16
  %4 = bitcast i8* %.repack.repack18.repack to i64*
  %.elt.elt19.elt = extractvalue { i64, i8* } %1, 0
  store i64 %.elt.elt19.elt, i64* %4, align 8
  %.repack.repack18.repack32 = getelementptr inbounds i8, i8* %0, i64 24
  %5 = bitcast i8* %.repack.repack18.repack32 to i8**
  %.elt.elt19.elt33 = extractvalue { i64, i8* } %1, 1
  store i8* %.elt.elt19.elt33, i8** %5, align 8
  %.repack.repack20.repack = getelementptr inbounds i8, i8* %0, i64 32
  %6 = bitcast i8* %.repack.repack20.repack to i64*
  store i64 0, i64* %6, align 8
  %.repack.repack20.repack30 = getelementptr inbounds i8, i8* %0, i64 40
  %7 = bitcast i8* %.repack.repack20.repack30 to i8**
  store i8* getelementptr inbounds ([1 x i8], [1 x i8]* @str_literal.61, i64 0, i64 0), i8** %7, align 8
  %.repack.repack22.repack = getelementptr inbounds i8, i8* %0, i64 48
  %8 = bitcast i8* %.repack.repack22.repack to i64*
  store i64 0, i64* %8, align 8
  %.repack.repack22.repack28 = getelementptr inbounds i8, i8* %0, i64 56
  %9 = bitcast i8* %.repack.repack22.repack28 to i8**
  store i8* getelementptr inbounds ([1 x i8], [1 x i8]* @str_literal.62, i64 0, i64 0), i8** %9, align 8
  %.repack.repack24 = getelementptr inbounds i8, i8* %0, i64 64
  %.repack14.repack = getelementptr inbounds i8, i8* %0, i64 80
  %10 = bitcast i8* %.repack14.repack to i64*
  %.elt15.elt = extractvalue { i64, i8* } %2, 0
  call void @llvm.memset.p0i8.i64(i8* nonnull %.repack.repack24, i8 0, i64 16, i32 8, i1 false)
  store i64 %.elt15.elt, i64* %10, align 8
  %.repack14.repack16 = getelementptr inbounds i8, i8* %0, i64 88
  %11 = bitcast i8* %.repack14.repack16 to i8**
  %.elt15.elt17 = extractvalue { i64, i8* } %2, 1
  store i8* %.elt15.elt17, i8** %11, align 8
  ret void
}

; Function Attrs: noinline uwtable
define void @"py_tuple_setitem.void.ptr[byte].int.ptr[byte]"(i8*, i64, i8*) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  call void @"PyTuple_SetItem.void.ptr[byte].int.ptr[byte]"(i8* %0, i64 %1, i8* %2)
  call void @"pyobj::exc_check.void"()
  ret void
}

; Function Attrs: noinline uwtable
define void @"PyTuple_SetItem.void.ptr[byte].int.ptr[byte]"(i8*, i64, i8*) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.unpack = load i64, i64* @seq.var.1847.0, align 8
  %3 = insertvalue { i64, i8* } undef, i64 %.unpack, 0
  %.unpack4 = load i8*, i8** @seq.var.1847.1, align 8
  %4 = insertvalue { i64, i8* } %3, i8* %.unpack4, 1
  %5 = call i8* @"dlsym.ptr[byte].str.str"({ i64, i8* } %4, { i64, i8* } { i64 15, i8* getelementptr inbounds ([16 x i8], [16 x i8]* @str_literal.65, i32 0, i32 0) })
  %6 = bitcast i8* %5 to void (i8*, i64, i8*)*
  call void %6(i8* %0, i64 %1, i8* %2)
  ret void
}

; Function Attrs: noinline uwtable
define i8* @"py_tuple_getitem.ptr[byte].ptr[byte].int"(i8*, i64) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %2 = call i8* @"PyTuple_GetItem.ptr[byte].ptr[byte].int"(i8* %0, i64 %1)
  call void @"pyobj::exc_check.void"()
  ret i8* %2
}

; Function Attrs: noinline uwtable
define i8* @"PyTuple_GetItem.ptr[byte].ptr[byte].int"(i8*, i64) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.unpack = load i64, i64* @seq.var.1847.0, align 8
  %2 = insertvalue { i64, i8* } undef, i64 %.unpack, 0
  %.unpack3 = load i8*, i8** @seq.var.1847.1, align 8
  %3 = insertvalue { i64, i8* } %2, i8* %.unpack3, 1
  %4 = call i8* @"dlsym.ptr[byte].str.str"({ i64, i8* } %3, { i64, i8* } { i64 15, i8* getelementptr inbounds ([16 x i8], [16 x i8]* @str_literal.66, i32 0, i32 0) })
  %5 = bitcast i8* %4 to i8* (i8*, i64)*
  %6 = call i8* %5(i8* %0, i64 %1)
  ret i8* %6
}

; Function Attrs: noinline uwtable
define void @"SAM::__init__.void.SAM.str"(i8* nocapture, { i64, i8* }) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %2 = call i8* @"str::c_str.ptr[byte].str"({ i64, i8* } %1)
  %3 = call i8* @"str::c_str.ptr[byte].str"({ i64, i8* } { i64 1, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @str_literal.68, i32 0, i32 0) })
  %4 = call i8* @hts_open(i8* %2, i8* %3)
  %5 = icmp eq i8* %4, null
  br i1 %5, label %6, label %17

; <label>:6:                                      ; preds = %preamble
  %7 = call { i64, i8* } @"str::__add__.str.str.str"({ i64, i8* } { i64 5, i8* getelementptr inbounds ([6 x i8], [6 x i8]* @str_literal.69, i32 0, i32 0) }, { i64, i8* } %1)
  %8 = call { i64, i8* } @"str::__add__.str.str.str"({ i64, i8* } %7, { i64, i8* } { i64 20, i8* getelementptr inbounds ([21 x i8], [21 x i8]* @str_literal.70, i32 0, i32 0) })
  %9 = call i8* @seq_alloc(i64 80)
  call void @"IOError::__init__.void.IOError.str"(i8* %9, { i64, i8* } %8)
  %.repack67.repack = getelementptr inbounds i8, i8* %9, i64 32
  %10 = bitcast i8* %.repack67.repack to i64*
  store i64 8, i64* %10, align 8
  %.repack67.repack77 = getelementptr inbounds i8, i8* %9, i64 40
  %11 = bitcast i8* %.repack67.repack77 to i8**
  store i8* getelementptr inbounds ([9 x i8], [9 x i8]* @str_literal.74, i64 0, i64 0), i8** %11, align 8
  %.repack69.repack = getelementptr inbounds i8, i8* %9, i64 48
  %12 = bitcast i8* %.repack69.repack to i64*
  store i64 59, i64* %12, align 8
  %.repack69.repack75 = getelementptr inbounds i8, i8* %9, i64 56
  %13 = bitcast i8* %.repack69.repack75 to i8**
  store i8* getelementptr inbounds ([60 x i8], [60 x i8]* @str_literal.75, i64 0, i64 0), i8** %13, align 8
  %.repack71 = getelementptr inbounds i8, i8* %9, i64 64
  %14 = bitcast i8* %.repack71 to i64*
  store i64 319, i64* %14, align 8
  %.repack73 = getelementptr inbounds i8, i8* %9, i64 72
  %15 = bitcast i8* %.repack73 to i64*
  store i64 13, i64* %15, align 8
  %16 = call i8* @seq_alloc_exc(i32 1341822379, i8* %9)
  call void @seq_throw(i8* %16)
  unreachable

; <label>:17:                                     ; preds = %preamble
  %18 = call i8* @sam_hdr_read(i8* nonnull %4)
  %19 = call i8* @bam_init1()
  %20 = call { i64, { { i64, i8* }, i64 }* } @seq_hts_get_targets(i8* %18)
  %21 = call i64 @"len[array[SAMHeaderTarget]].int.array[SAMHeaderTarget]"({ i64, { { i64, i8* }, i64 }* } %20)
  %22 = call i8* @seq_alloc(i64 24)
  call void @"list[SAMHeaderTarget]::__init__.void.list[SAMHeaderTarget].array[SAMHeaderTarget].int"(i8* %22, { i64, { { i64, i8* }, i64 }* } %20, i64 %21)
  %.repack = bitcast i8* %0 to i8**
  store i8* %4, i8** %.repack, align 8
  %.repack42 = getelementptr inbounds i8, i8* %0, i64 8
  %23 = bitcast i8* %.repack42 to i8**
  store i8* %18, i8** %23, align 8
  %.repack44 = getelementptr inbounds i8, i8* %0, i64 16
  %24 = bitcast i8* %.repack44 to i8**
  store i8* %19, i8** %24, align 8
  %.repack46 = getelementptr inbounds i8, i8* %0, i64 24
  %25 = bitcast i8* %.repack46 to i8**
  store i8* %22, i8** %25, align 8
  ret void
}

; Function Attrs: noinline uwtable
declare i8* @hts_open(i8*, i8*) local_unnamed_addr #0

; Function Attrs: noinline norecurse nounwind uwtable
define void @"IOError::__init__.void.IOError.str"(i8* nocapture, { i64, i8* }) local_unnamed_addr #7 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.repack.repack = bitcast i8* %0 to i64*
  store i64 7, i64* %.repack.repack, align 8
  %.repack.repack19 = getelementptr inbounds i8, i8* %0, i64 8
  %2 = bitcast i8* %.repack.repack19 to i8**
  store i8* getelementptr inbounds ([8 x i8], [8 x i8]* @str_literal.71, i64 0, i64 0), i8** %2, align 8
  %.repack3.repack = getelementptr inbounds i8, i8* %0, i64 16
  %3 = bitcast i8* %.repack3.repack to i64*
  %.elt4.elt = extractvalue { i64, i8* } %1, 0
  store i64 %.elt4.elt, i64* %3, align 8
  %.repack3.repack17 = getelementptr inbounds i8, i8* %0, i64 24
  %4 = bitcast i8* %.repack3.repack17 to i8**
  %.elt4.elt18 = extractvalue { i64, i8* } %1, 1
  store i8* %.elt4.elt18, i8** %4, align 8
  %.repack5.repack = getelementptr inbounds i8, i8* %0, i64 32
  %5 = bitcast i8* %.repack5.repack to i64*
  store i64 0, i64* %5, align 8
  %.repack5.repack15 = getelementptr inbounds i8, i8* %0, i64 40
  %6 = bitcast i8* %.repack5.repack15 to i8**
  store i8* getelementptr inbounds ([1 x i8], [1 x i8]* @str_literal.72, i64 0, i64 0), i8** %6, align 8
  %.repack7.repack = getelementptr inbounds i8, i8* %0, i64 48
  %7 = bitcast i8* %.repack7.repack to i64*
  store i64 0, i64* %7, align 8
  %.repack7.repack13 = getelementptr inbounds i8, i8* %0, i64 56
  %8 = bitcast i8* %.repack7.repack13 to i8**
  store i8* getelementptr inbounds ([1 x i8], [1 x i8]* @str_literal.73, i64 0, i64 0), i8** %8, align 8
  %.repack9 = getelementptr inbounds i8, i8* %0, i64 64
  call void @llvm.memset.p0i8.i64(i8* nonnull %.repack9, i8 0, i64 16, i32 8, i1 false)
  ret void
}

; Function Attrs: noinline uwtable
declare i8* @sam_hdr_read(i8*) local_unnamed_addr #0

; Function Attrs: noinline uwtable
declare i8* @bam_init1() local_unnamed_addr #0

; Function Attrs: noinline uwtable
declare { i64, { { i64, i8* }, i64 }* } @seq_hts_get_targets(i8*) local_unnamed_addr #0

; Function Attrs: noinline norecurse nounwind readnone uwtable
define i64 @"len[array[SAMHeaderTarget]].int.array[SAMHeaderTarget]"({ i64, { { i64, i8* }, i64 }* }) local_unnamed_addr #2 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %1 = call i64 @seq.magic.__len__.5({ i64, { { i64, i8* }, i64 }* } %0)
  ret i64 %1
}

; Function Attrs: noinline norecurse nounwind readnone uwtable
define i64 @seq.magic.__len__.5({ i64, { { i64, i8* }, i64 }* }) local_unnamed_addr #2 {
entry:
  %1 = extractvalue { i64, { { i64, i8* }, i64 }* } %0, 0
  ret i64 %1
}

; Function Attrs: noinline norecurse nounwind uwtable
define void @"list[SAMHeaderTarget]::__init__.void.list[SAMHeaderTarget].array[SAMHeaderTarget].int"(i8* nocapture, { i64, { { i64, i8* }, i64 }* }, i64) local_unnamed_addr #7 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.repack.repack = bitcast i8* %0 to i64*
  %.elt.elt = extractvalue { i64, { { i64, i8* }, i64 }* } %1, 0
  store i64 %.elt.elt, i64* %.repack.repack, align 8
  %.repack.repack8 = getelementptr inbounds i8, i8* %0, i64 8
  %3 = bitcast i8* %.repack.repack8 to { { i64, i8* }, i64 }**
  %.elt.elt9 = extractvalue { i64, { { i64, i8* }, i64 }* } %1, 1
  store { { i64, i8* }, i64 }* %.elt.elt9, { { i64, i8* }, i64 }** %3, align 8
  %.repack6 = getelementptr inbounds i8, i8* %0, i64 16
  %4 = bitcast i8* %.repack6 to i64*
  store i64 %2, i64* %4, align 8
  ret void
}

; Function Attrs: noinline nounwind uwtable
define noalias i8* @"SAM::__iter__.generator[SAMRecord].SAM"(i8* nocapture) local_unnamed_addr #3 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %1 = call i8* @seq_alloc(i64 48)
  %resume.addr = bitcast i8* %1 to void (%"SAM::__iter__.generator[SAMRecord].SAM.Frame"*)**
  store void (%"SAM::__iter__.generator[SAMRecord].SAM.Frame"*)* @"SAM::__iter__.generator[SAMRecord].SAM.resume", void (%"SAM::__iter__.generator[SAMRecord].SAM.Frame"*)** %resume.addr, align 8
  %destroy.addr = getelementptr inbounds i8, i8* %1, i64 8
  %2 = bitcast i8* %destroy.addr to void (%"SAM::__iter__.generator[SAMRecord].SAM.Frame"*)**
  store void (%"SAM::__iter__.generator[SAMRecord].SAM.Frame"*)* @"SAM::__iter__.generator[SAMRecord].SAM.destroy", void (%"SAM::__iter__.generator[SAMRecord].SAM.Frame"*)** %2, align 8
  %.spill.addr = getelementptr inbounds i8, i8* %1, i64 32
  %3 = bitcast i8* %.spill.addr to i8**
  store i8* %0, i8** %3, align 8
  %index.addr25 = getelementptr inbounds i8, i8* %1, i64 24
  %4 = bitcast i8* %index.addr25 to i2*
  store i2 0, i2* %4, align 1
  ret i8* %1
}

; Function Attrs: noinline nounwind uwtable
define noalias i8* @"SAM::_iter.generator[ptr[byte]].SAM"(i8* nocapture) local_unnamed_addr #3 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %1 = call i8* @seq_alloc(i64 40)
  %resume.addr = bitcast i8* %1 to void (%"SAM::_iter.generator[ptr[byte]].SAM.Frame"*)**
  store void (%"SAM::_iter.generator[ptr[byte]].SAM.Frame"*)* @"SAM::_iter.generator[ptr[byte]].SAM.resume", void (%"SAM::_iter.generator[ptr[byte]].SAM.Frame"*)** %resume.addr, align 8
  %destroy.addr = getelementptr inbounds i8, i8* %1, i64 8
  %2 = bitcast i8* %destroy.addr to void (%"SAM::_iter.generator[ptr[byte]].SAM.Frame"*)**
  store void (%"SAM::_iter.generator[ptr[byte]].SAM.Frame"*)* @"SAM::_iter.generator[ptr[byte]].SAM.destroy", void (%"SAM::_iter.generator[ptr[byte]].SAM.Frame"*)** %2, align 8
  %.spill.addr = getelementptr inbounds i8, i8* %1, i64 32
  %3 = bitcast i8* %.spill.addr to i8**
  store i8* %0, i8** %3, align 8
  %index.addr96 = getelementptr inbounds i8, i8* %1, i64 24
  %4 = bitcast i8* %index.addr96 to i2*
  store i2 0, i2* %4, align 1
  ret i8* %1
}

; Function Attrs: noinline uwtable
define void @"SAM::_ensure_open.void.SAM"(i8* nocapture readonly) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.elt = bitcast i8* %0 to i8**
  %.unpack = load i8*, i8** %.elt, align 8
  %1 = icmp eq i8* %.unpack, null
  br i1 %1, label %2, label %11

; <label>:2:                                      ; preds = %preamble
  %3 = call i8* @seq_alloc(i64 80)
  call void @"IOError::__init__.void.IOError.str"(i8* %3, { i64, i8* } { i64 32, i8* getelementptr inbounds ([33 x i8], [33 x i8]* @str_literal.77, i32 0, i32 0) })
  %.repack28.repack = getelementptr inbounds i8, i8* %3, i64 32
  %4 = bitcast i8* %.repack28.repack to i64*
  store i64 12, i64* %4, align 8
  %.repack28.repack38 = getelementptr inbounds i8, i8* %3, i64 40
  %5 = bitcast i8* %.repack28.repack38 to i8**
  store i8* getelementptr inbounds ([13 x i8], [13 x i8]* @str_literal.78, i64 0, i64 0), i8** %5, align 8
  %.repack30.repack = getelementptr inbounds i8, i8* %3, i64 48
  %6 = bitcast i8* %.repack30.repack to i64*
  store i64 59, i64* %6, align 8
  %.repack30.repack36 = getelementptr inbounds i8, i8* %3, i64 56
  %7 = bitcast i8* %.repack30.repack36 to i8**
  store i8* getelementptr inbounds ([60 x i8], [60 x i8]* @str_literal.79, i64 0, i64 0), i8** %7, align 8
  %.repack32 = getelementptr inbounds i8, i8* %3, i64 64
  %8 = bitcast i8* %.repack32 to i64*
  store i64 333, i64* %8, align 8
  %.repack34 = getelementptr inbounds i8, i8* %3, i64 72
  %9 = bitcast i8* %.repack34 to i64*
  store i64 13, i64* %9, align 8
  %10 = call i8* @seq_alloc_exc(i32 1341822379, i8* %3)
  call void @seq_throw(i8* %10)
  unreachable

; <label>:11:                                     ; preds = %preamble
  ret void
}

; Function Attrs: noinline uwtable
declare i32 @sam_read1(i8*, i8*, i8*) local_unnamed_addr #0

; Function Attrs: noinline nounwind uwtable
declare { i64, i8* } @seq_str_int(i64) local_unnamed_addr #3

; Function Attrs: noinline uwtable
define void @"SAM::close.void.SAM"(i8* nocapture) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.elt11 = getelementptr inbounds i8, i8* %0, i64 16
  %1 = bitcast i8* %.elt11 to i8**
  %.unpack12 = load i8*, i8** %1, align 8
  %2 = icmp eq i8* %.unpack12, null
  br i1 %2, label %4, label %3

; <label>:3:                                      ; preds = %preamble
  call void @bam_destroy1(i8* nonnull %.unpack12)
  br label %4

; <label>:4:                                      ; preds = %3, %preamble
  %.elt15 = getelementptr inbounds i8, i8* %0, i64 8
  %5 = bitcast i8* %.elt15 to i8**
  %.unpack16 = load i8*, i8** %5, align 8
  %6 = icmp eq i8* %.unpack16, null
  br i1 %6, label %8, label %7

; <label>:7:                                      ; preds = %4
  call void @bam_hdr_destroy(i8* nonnull %.unpack16)
  br label %8

; <label>:8:                                      ; preds = %7, %4
  %.elt = bitcast i8* %0 to i8**
  %.unpack = load i8*, i8** %.elt, align 8
  %9 = icmp eq i8* %.unpack, null
  br i1 %9, label %11, label %10

; <label>:10:                                     ; preds = %8
  call void @hts_close(i8* nonnull %.unpack)
  br label %11

; <label>:11:                                     ; preds = %10, %8
  call void @llvm.memset.p0i8.i64(i8* nonnull %0, i8 0, i64 24, i32 8, i1 false)
  ret void
}

; Function Attrs: noinline uwtable
declare void @bam_destroy1(i8*) local_unnamed_addr #0

; Function Attrs: noinline uwtable
declare void @bam_hdr_destroy(i8*) local_unnamed_addr #0

; Function Attrs: noinline uwtable
declare void @hts_close(i8*) local_unnamed_addr #0

; Function Attrs: noinline uwtable
define void @"SAMRecord::__init__.void.SAMRecord.ptr[byte]"(i8* nocapture, i8*) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %2 = call { i64, i8* } @seq_hts_get_name(i8* %1)
  %.unpack313.elt = getelementptr inbounds i8, i8* %0, i64 16
  %3 = bitcast i8* %.unpack313.elt to i64*
  %.unpack313.elt331 = getelementptr inbounds i8, i8* %0, i64 24
  %.unpack315.elt = getelementptr inbounds i8, i8* %0, i64 32
  %4 = bitcast i8* %.unpack315.elt to i64*
  %.unpack315.elt328 = getelementptr inbounds i8, i8* %0, i64 40
  %.unpack317.elt = getelementptr inbounds i8, i8* %0, i64 48
  %.unpack317.elt325 = getelementptr inbounds i8, i8* %0, i64 56
  %5 = bitcast i8* %.unpack317.elt325 to i64*
  %.elt318 = getelementptr inbounds i8, i8* %0, i64 64
  %6 = bitcast i8* %.elt318 to { i32, i32, i8, i16, i32, i32, i32 }*
  %.unpack321.elt = getelementptr inbounds i8, i8* %0, i64 88
  %7 = bitcast i8* %.unpack321.elt to i64*
  %.unpack321.elt322 = getelementptr inbounds i8, i8* %0, i64 96
  %.repack.repack = bitcast i8* %0 to i64*
  %.elt.elt = extractvalue { i64, i8* } %2, 0
  store i64 %.elt.elt, i64* %.repack.repack, align 8
  %.repack.repack352 = getelementptr inbounds i8, i8* %0, i64 8
  %8 = bitcast i8* %.repack.repack352 to i8**
  %.elt.elt353 = extractvalue { i64, i8* } %2, 1
  store i8* %.elt.elt353, i8** %8, align 8
  %9 = call { i64, i8* } @seq_hts_get_seq(i8* %1)
  %.elt381.elt = extractvalue { i64, i8* } %9, 0
  store i64 %.elt381.elt, i64* %3, align 8
  %10 = bitcast i8* %.unpack313.elt331 to i8**
  %.elt381.elt397 = extractvalue { i64, i8* } %9, 1
  store i8* %.elt381.elt397, i8** %10, align 8
  %11 = call { i64, i8* } @seq_hts_get_qual(i8* %1)
  %.elt433.elt = extractvalue { i64, i8* } %11, 0
  store i64 %.elt433.elt, i64* %4, align 8
  %12 = bitcast i8* %.unpack315.elt328 to i8**
  %.elt433.elt445 = extractvalue { i64, i8* } %11, 1
  store i8* %.elt433.elt445, i8** %12, align 8
  %13 = call { i32*, i64 } @seq_hts_get_cigar(i8* %1)
  %14 = bitcast i8* %.unpack317.elt to i32**
  %.elt485.elt = extractvalue { i32*, i64 } %13, 0
  store i32* %.elt485.elt, i32** %14, align 8
  %.elt485.elt493 = extractvalue { i32*, i64 } %13, 1
  store i64 %.elt485.elt493, i64* %5, align 8
  %15 = call { i64, i8* } @seq_hts_get_aux(i8* %1)
  %.elt539.elt = extractvalue { i64, i8* } %15, 0
  store i64 %.elt539.elt, i64* %7, align 8
  %16 = bitcast i8* %.unpack321.elt322 to i8**
  %.elt539.elt541 = extractvalue { i64, i8* } %15, 1
  store i8* %.elt539.elt541, i8** %16, align 8
  %.elt = bitcast i8* %1 to i32*
  %.unpack = load i32, i32* %.elt, align 4
  %.elt552 = getelementptr inbounds i8, i8* %1, i64 4
  %17 = bitcast i8* %.elt552 to i32*
  %.unpack553 = load i32, i32* %17, align 4
  %18 = getelementptr inbounds i8, i8* %1, i64 10
  %.unpack557 = load i8, i8* %18, align 2
  %.elt560 = getelementptr inbounds i8, i8* %1, i64 12
  %19 = bitcast i8* %.elt560 to i16*
  %.unpack561 = load i16, i16* %19, align 4
  %.elt568 = getelementptr inbounds i8, i8* %1, i64 24
  %20 = bitcast i8* %.elt568 to i32*
  %.unpack569 = load i32, i32* %20, align 4
  %.elt570 = getelementptr inbounds i8, i8* %1, i64 28
  %21 = bitcast i8* %.elt570 to i32*
  %.unpack571 = load i32, i32* %21, align 4
  %.elt572 = getelementptr inbounds i8, i8* %1, i64 32
  %22 = bitcast i8* %.elt572 to i32*
  %.unpack573 = load i32, i32* %22, align 4
  %23 = insertvalue { i32, i32, i8, i16, i32, i32, i32 } zeroinitializer, i32 %.unpack, 0
  %24 = insertvalue { i32, i32, i8, i16, i32, i32, i32 } %23, i32 %.unpack553, 1
  %25 = insertvalue { i32, i32, i8, i16, i32, i32, i32 } %24, i8 %.unpack557, 2
  %26 = insertvalue { i32, i32, i8, i16, i32, i32, i32 } %25, i16 %.unpack561, 3
  %27 = insertvalue { i32, i32, i8, i16, i32, i32, i32 } %26, i32 %.unpack569, 4
  %28 = insertvalue { i32, i32, i8, i16, i32, i32, i32 } %27, i32 %.unpack571, 5
  %29 = insertvalue { i32, i32, i8, i16, i32, i32, i32 } %28, i32 %.unpack573, 6
  store { i32, i32, i8, i16, i32, i32, i32 } %29, { i32, i32, i8, i16, i32, i32, i32 }* %6, align 8
  store i64 %.elt539.elt, i64* %7, align 8
  store i8* %.elt539.elt541, i8** %16, align 8
  ret void
}

; Function Attrs: noinline uwtable
declare { i64, i8* } @seq_hts_get_name(i8*) local_unnamed_addr #0

; Function Attrs: noinline uwtable
declare { i64, i8* } @seq_hts_get_seq(i8*) local_unnamed_addr #0

; Function Attrs: noinline uwtable
declare { i64, i8* } @seq_hts_get_qual(i8*) local_unnamed_addr #0

; Function Attrs: noinline uwtable
declare { i32*, i64 } @seq_hts_get_cigar(i8*) local_unnamed_addr #0

; Function Attrs: noinline uwtable
declare { i64, i8* } @seq_hts_get_aux(i8*) local_unnamed_addr #0

; Function Attrs: noinline norecurse nounwind readonly uwtable
define { i64, i8* } @"SAMRecord::name.str.SAMRecord"(i8* nocapture readonly) local_unnamed_addr #8 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
preamble:
  %.unpack.elt = bitcast i8* %0 to i64*
  %.unpack.unpack = load i64, i64* %.unpack.elt, align 8
  %1 = insertvalue { i64, i8* } undef, i64 %.unpack.unpack, 0
  %.unpack.elt23 = getelementptr inbounds i8, i8* %0, i64 8
  %2 = bitcast i8* %.unpack.elt23 to i8**
  %.unpack.unpack24 = load i8*, i8** %2, align 8
  %.unpack25 = insertvalue { i64, i8* } %1, i8* %.unpack.unpack24, 1
  ret { i64, i8* } %.unpack25
}

; Function Attrs: noinline nounwind uwtable
declare void @seq_print({ i64, i8* }) local_unnamed_addr #3

; Function Attrs: noinline uwtable
define i32 @main(i32 %argc, i8** nocapture readonly %argv) local_unnamed_addr #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
entry:
  %0 = zext i32 %argc to i64
  %1 = shl nuw nsw i64 %0, 4
  %2 = call i8* @seq_alloc(i64 %1)
  %3 = bitcast i8* %2 to { i64, i8* }*
  %4 = icmp sgt i32 %argc, 0
  br i1 %4, label %body.preheader, label %exit

body.preheader:                                   ; preds = %entry
  br label %body

body:                                             ; preds = %body, %body.preheader
  %indvars.iv = phi i64 [ %indvars.iv.next, %body ], [ 0, %body.preheader ]
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %5 = getelementptr i8*, i8** %argv, i64 %indvars.iv
  %6 = load i8*, i8** %5, align 8
  %7 = call i64 @strlen(i8* %6)
  %.repack = getelementptr inbounds { i64, i8* }, { i64, i8* }* %3, i64 %indvars.iv, i32 0
  store i64 %7, i64* %.repack, align 8
  %.repack1 = getelementptr inbounds { i64, i8* }, { i64, i8* }* %3, i64 %indvars.iv, i32 1
  store i8* %6, i8** %.repack1, align 8
  %exitcond = icmp eq i64 %indvars.iv.next, %0
  br i1 %exitcond, label %exit, label %body

exit:                                             ; preds = %body, %entry
  call void @seq_init()
  invoke void @seq.main()
          to label %normal unwind label %unwind

normal:                                           ; preds = %exit
  ret i32 0

unwind:                                           ; preds = %exit
  %8 = landingpad { i8*, i32 }
          cleanup
          catch { i32 }* @"seq.typeidx.<all>"
  %9 = extractvalue { i8*, i32 } %8, 0
  call void @seq_terminate(i8* %9)
  unreachable
}

; Function Attrs: noinline noreturn uwtable
declare void @seq_terminate(i8*) local_unnamed_addr #6

; Function Attrs: argmemonly nounwind
declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i32, i1) #5

; Function Attrs: noinline norecurse nounwind readnone uwtable
define void @coro.devirt.trigger(i8* nocapture readnone) local_unnamed_addr #2 {
entry:
  ret void
}

; Function Attrs: noinline norecurse nounwind uwtable
define fastcc void @"range::__iter__.generator[int].range.resume"(%"range::__iter__.generator[int].range.Frame"* nocapture %FramePtr) #7 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
entry.resume:
  %promise = getelementptr inbounds %"range::__iter__.generator[int].range.Frame", %"range::__iter__.generator[int].range.Frame"* %FramePtr, i64 0, i32 2
  %index.addr = getelementptr inbounds %"range::__iter__.generator[int].range.Frame", %"range::__iter__.generator[int].range.Frame"* %FramePtr, i64 0, i32 3
  %index = load i2, i2* %index.addr, align 1
  switch i2 %index, label %unreachable [
    i2 0, label %AfterCoroSuspend
    i2 1, label %while
    i2 -2, label %while1
  ]

AfterCoroSuspend:                                 ; preds = %entry.resume
  %.fca.2.extract47.reload.addr113 = getelementptr inbounds %"range::__iter__.generator[int].range.Frame", %"range::__iter__.generator[int].range.Frame"* %FramePtr, i64 0, i32 6
  %.fca.2.extract47.reload114 = load i64, i64* %.fca.2.extract47.reload.addr113, align 8
  %0 = icmp sgt i64 %.fca.2.extract47.reload114, 0
  %.fca.1.extract46.reload.addr105 = getelementptr inbounds %"range::__iter__.generator[int].range.Frame", %"range::__iter__.generator[int].range.Frame"* %FramePtr, i64 0, i32 5
  %.fca.1.extract46.reload106 = load i64, i64* %.fca.1.extract46.reload.addr105, align 8
  %.fca.0.extract45.reload.addr103 = getelementptr inbounds %"range::__iter__.generator[int].range.Frame", %"range::__iter__.generator[int].range.Frame"* %FramePtr, i64 0, i32 4
  %.fca.0.extract45.reload104 = load i64, i64* %.fca.0.extract45.reload.addr103, align 8
  br i1 %0, label %1, label %5

; <label>:1:                                      ; preds = %AfterCoroSuspend
  %2 = icmp slt i64 %.fca.0.extract45.reload104, %.fca.1.extract46.reload106
  br i1 %2, label %.AfterCoroSuspend91_crit_edge, label %CoroSave95

.AfterCoroSuspend91_crit_edge:                    ; preds = %1
  %.pre = getelementptr inbounds %"range::__iter__.generator[int].range.Frame", %"range::__iter__.generator[int].range.Frame"* %FramePtr, i64 0, i32 7
  br label %AfterCoroSuspend91

while:                                            ; preds = %entry.resume
  %.083.reload.addr1 = getelementptr inbounds %"range::__iter__.generator[int].range.Frame", %"range::__iter__.generator[int].range.Frame"* %FramePtr, i64 0, i32 7
  %.083.reload2 = load i64, i64* %.083.reload.addr1, align 8
  %.fca.2.extract47.reload.addr3 = getelementptr inbounds %"range::__iter__.generator[int].range.Frame", %"range::__iter__.generator[int].range.Frame"* %FramePtr, i64 0, i32 6
  %.fca.2.extract47.reload4 = load i64, i64* %.fca.2.extract47.reload.addr3, align 8
  %3 = add i64 %.fca.2.extract47.reload4, %.083.reload2
  %.fca.1.extract46.reload.addr109 = getelementptr inbounds %"range::__iter__.generator[int].range.Frame", %"range::__iter__.generator[int].range.Frame"* %FramePtr, i64 0, i32 5
  %.fca.1.extract46.reload110 = load i64, i64* %.fca.1.extract46.reload.addr109, align 8
  %4 = icmp slt i64 %3, %.fca.1.extract46.reload110
  br i1 %4, label %AfterCoroSuspend91, label %CoroSave95

AfterCoroSuspend91:                               ; preds = %while, %.AfterCoroSuspend91_crit_edge
  %.083.spill.addr.pre-phi = phi i64* [ %.pre, %.AfterCoroSuspend91_crit_edge ], [ %.083.reload.addr1, %while ]
  %.083 = phi i64 [ %.fca.0.extract45.reload104, %.AfterCoroSuspend91_crit_edge ], [ %3, %while ]
  store i64 %.083, i64* %.083.spill.addr.pre-phi, align 8
  store i64 %.083, i64* %promise, align 8
  store i2 1, i2* %index.addr, align 1
  br label %CoroEnd

; <label>:5:                                      ; preds = %AfterCoroSuspend
  %6 = icmp sgt i64 %.fca.0.extract45.reload104, %.fca.1.extract46.reload106
  br i1 %6, label %.AfterCoroSuspend94_crit_edge, label %CoroSave95

.AfterCoroSuspend94_crit_edge:                    ; preds = %5
  %.pre11 = getelementptr inbounds %"range::__iter__.generator[int].range.Frame", %"range::__iter__.generator[int].range.Frame"* %FramePtr, i64 0, i32 8
  br label %AfterCoroSuspend94

while1:                                           ; preds = %entry.resume
  %.184.reload.addr6 = getelementptr inbounds %"range::__iter__.generator[int].range.Frame", %"range::__iter__.generator[int].range.Frame"* %FramePtr, i64 0, i32 8
  %.184.reload7 = load i64, i64* %.184.reload.addr6, align 8
  %.fca.2.extract47.reload.addr1118 = getelementptr inbounds %"range::__iter__.generator[int].range.Frame", %"range::__iter__.generator[int].range.Frame"* %FramePtr, i64 0, i32 6
  %.fca.2.extract47.reload1129 = load i64, i64* %.fca.2.extract47.reload.addr1118, align 8
  %7 = add i64 %.fca.2.extract47.reload1129, %.184.reload7
  %.fca.1.extract46.reload.addr107 = getelementptr inbounds %"range::__iter__.generator[int].range.Frame", %"range::__iter__.generator[int].range.Frame"* %FramePtr, i64 0, i32 5
  %.fca.1.extract46.reload108 = load i64, i64* %.fca.1.extract46.reload.addr107, align 8
  %8 = icmp sgt i64 %7, %.fca.1.extract46.reload108
  br i1 %8, label %AfterCoroSuspend94, label %CoroSave95

AfterCoroSuspend94:                               ; preds = %while1, %.AfterCoroSuspend94_crit_edge
  %.184.spill.addr.pre-phi = phi i64* [ %.pre11, %.AfterCoroSuspend94_crit_edge ], [ %.184.reload.addr6, %while1 ]
  %.184 = phi i64 [ %.fca.0.extract45.reload104, %.AfterCoroSuspend94_crit_edge ], [ %7, %while1 ]
  store i64 %.184, i64* %.184.spill.addr.pre-phi, align 8
  store i64 %.184, i64* %promise, align 8
  store i2 -2, i2* %index.addr, align 1
  br label %CoroEnd

CoroSave95:                                       ; preds = %while1, %5, %while, %1
  %ResumeFn.addr = getelementptr inbounds %"range::__iter__.generator[int].range.Frame", %"range::__iter__.generator[int].range.Frame"* %FramePtr, i64 0, i32 0
  store void (%"range::__iter__.generator[int].range.Frame"*)* null, void (%"range::__iter__.generator[int].range.Frame"*)** %ResumeFn.addr, align 8
  br label %CoroEnd

CoroEnd:                                          ; preds = %CoroSave95, %AfterCoroSuspend94, %AfterCoroSuspend91
  ret void

unreachable:                                      ; preds = %entry.resume
  unreachable
}

; Function Attrs: noinline norecurse nounwind readnone uwtable
define fastcc void @"range::__iter__.generator[int].range.destroy"(%"range::__iter__.generator[int].range.Frame"* nocapture %FramePtr) #2 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
entry.destroy:
  ret void
}

; Function Attrs: noinline norecurse nounwind readnone uwtable
define fastcc void @"range::__iter__.generator[int].range.cleanup"(%"range::__iter__.generator[int].range.Frame"* nocapture %FramePtr) local_unnamed_addr #2 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
entry.cleanup:
  ret void
}

; Function Attrs: noinline uwtable
define fastcc void @"SAM::_iter.generator[ptr[byte]].SAM.resume"(%"SAM::_iter.generator[ptr[byte]].SAM.Frame"* nocapture %FramePtr) #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
entry.resume:
  %promise = getelementptr inbounds %"SAM::_iter.generator[ptr[byte]].SAM.Frame", %"SAM::_iter.generator[ptr[byte]].SAM.Frame"* %FramePtr, i64 0, i32 2
  %index.addr = getelementptr inbounds %"SAM::_iter.generator[ptr[byte]].SAM.Frame", %"SAM::_iter.generator[ptr[byte]].SAM.Frame"* %FramePtr, i64 0, i32 3
  %index = load i2, i2* %index.addr, align 1
  %switch = icmp eq i2 %index, 0
  %.reload.addr94 = getelementptr inbounds %"SAM::_iter.generator[ptr[byte]].SAM.Frame", %"SAM::_iter.generator[ptr[byte]].SAM.Frame"* %FramePtr, i64 0, i32 4
  %.reload95 = load i8*, i8** %.reload.addr94, align 8
  br i1 %switch, label %AfterCoroSuspend, label %while

AfterCoroSuspend:                                 ; preds = %entry.resume
  call void @"SAM::_ensure_open.void.SAM"(i8* %.reload95)
  %.elt = bitcast i8* %.reload95 to i8**
  %.unpack67 = load i8*, i8** %.elt, align 8
  %.elt10 = getelementptr inbounds i8, i8* %.reload95, i64 8
  %0 = bitcast i8* %.elt10 to i8**
  %.unpack1168 = load i8*, i8** %0, align 8
  %.elt12 = getelementptr inbounds i8, i8* %.reload95, i64 16
  %1 = bitcast i8* %.elt12 to i8**
  %.unpack1369 = load i8*, i8** %1, align 8
  %2 = call i32 @sam_read1(i8* %.unpack67, i8* %.unpack1168, i8* %.unpack1369)
  %3 = icmp sgt i32 %2, -1
  br i1 %3, label %AfterCoroSuspend76, label %while._crit_edge

while:                                            ; preds = %entry.resume
  %.elt1285 = getelementptr inbounds i8, i8* %.reload95, i64 16
  %.elt1083 = getelementptr inbounds i8, i8* %.reload95, i64 8
  %4 = bitcast i8* %.elt1285 to i8**
  %5 = bitcast i8* %.elt1083 to i8**
  %.elt82 = bitcast i8* %.reload95 to i8**
  %.unpack = load i8*, i8** %.elt82, align 8
  %.unpack11 = load i8*, i8** %5, align 8
  %.unpack13 = load i8*, i8** %4, align 8
  %6 = call i32 @sam_read1(i8* %.unpack, i8* %.unpack11, i8* %.unpack13)
  %7 = icmp sgt i32 %6, -1
  br i1 %7, label %AfterCoroSuspend76, label %while._crit_edge

AfterCoroSuspend76:                               ; preds = %while, %AfterCoroSuspend
  %.reload87 = load i8*, i8** %.reload.addr94, align 8
  %.elt1284 = getelementptr inbounds i8, i8* %.reload87, i64 16
  %8 = bitcast i8** %promise to i64*
  %9 = bitcast i8* %.elt1284 to i64*
  %.unpack6063 = load i64, i64* %9, align 8
  store i64 %.unpack6063, i64* %8, align 8
  store i2 1, i2* %index.addr, align 1
  br label %CoroEnd

while._crit_edge:                                 ; preds = %while, %AfterCoroSuspend
  %.lcssa65 = phi i32 [ %2, %AfterCoroSuspend ], [ %6, %while ]
  %10 = icmp eq i32 %.lcssa65, -1
  br i1 %10, label %exit, label %11

; <label>:11:                                     ; preds = %while._crit_edge
  %.lcssa = sext i32 %.lcssa65 to i64
  %12 = call { i64, i8* } @seq_str_int(i64 %.lcssa)
  %13 = call { i64, i8* } @"str::__add__.str.str.str"({ i64, i8* } { i64 29, i8* getelementptr inbounds ([30 x i8], [30 x i8]* @str_literal.80, i32 0, i32 0) }, { i64, i8* } %12)
  %14 = call i8* @seq_alloc(i64 80)
  call void @"IOError::__init__.void.IOError.str"(i8* %14, { i64, i8* } %13)
  %.repack37.repack = getelementptr inbounds i8, i8* %14, i64 32
  %15 = bitcast i8* %.repack37.repack to i64*
  store i64 5, i64* %15, align 8
  %.repack37.repack47 = getelementptr inbounds i8, i8* %14, i64 40
  %16 = bitcast i8* %.repack37.repack47 to i8**
  store i8* getelementptr inbounds ([6 x i8], [6 x i8]* @str_literal.81, i64 0, i64 0), i8** %16, align 8
  %.repack39.repack = getelementptr inbounds i8, i8* %14, i64 48
  %17 = bitcast i8* %.repack39.repack to i64*
  store i64 59, i64* %17, align 8
  %.repack39.repack45 = getelementptr inbounds i8, i8* %14, i64 56
  %18 = bitcast i8* %.repack39.repack45 to i8**
  store i8* getelementptr inbounds ([60 x i8], [60 x i8]* @str_literal.82, i64 0, i64 0), i8** %18, align 8
  %.repack41 = getelementptr inbounds i8, i8* %14, i64 64
  %19 = bitcast i8* %.repack41 to i64*
  store i64 344, i64* %19, align 8
  %.repack43 = getelementptr inbounds i8, i8* %14, i64 72
  %20 = bitcast i8* %.repack43 to i64*
  store i64 17, i64* %20, align 8
  %21 = call i8* @seq_alloc_exc(i32 1341822379, i8* %14)
  call void @seq_throw(i8* %21)
  unreachable

exit:                                             ; preds = %while._crit_edge
  %.reload93 = load i8*, i8** %.reload.addr94, align 8
  call void @"SAM::close.void.SAM"(i8* nonnull %.reload93)
  %ResumeFn.addr = getelementptr inbounds %"SAM::_iter.generator[ptr[byte]].SAM.Frame", %"SAM::_iter.generator[ptr[byte]].SAM.Frame"* %FramePtr, i64 0, i32 0
  store void (%"SAM::_iter.generator[ptr[byte]].SAM.Frame"*)* null, void (%"SAM::_iter.generator[ptr[byte]].SAM.Frame"*)** %ResumeFn.addr, align 8
  br label %CoroEnd

CoroEnd:                                          ; preds = %exit, %AfterCoroSuspend76
  ret void
}

; Function Attrs: noinline norecurse nounwind readnone uwtable
define fastcc void @"SAM::_iter.generator[ptr[byte]].SAM.destroy"(%"SAM::_iter.generator[ptr[byte]].SAM.Frame"* nocapture %FramePtr) #2 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
entry.destroy:
  ret void
}

; Function Attrs: noinline norecurse nounwind readnone uwtable
define fastcc void @"SAM::_iter.generator[ptr[byte]].SAM.cleanup"(%"SAM::_iter.generator[ptr[byte]].SAM.Frame"* nocapture %FramePtr) local_unnamed_addr #2 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
entry.cleanup:
  ret void
}

; Function Attrs: noinline uwtable
define fastcc void @"SAM::__iter__.generator[SAMRecord].SAM.resume"(%"SAM::__iter__.generator[SAMRecord].SAM.Frame"* nocapture %FramePtr) #0 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
entry.resume:
  %promise = getelementptr inbounds %"SAM::__iter__.generator[SAMRecord].SAM.Frame", %"SAM::__iter__.generator[SAMRecord].SAM.Frame"* %FramePtr, i64 0, i32 2
  %index.addr = getelementptr inbounds %"SAM::__iter__.generator[SAMRecord].SAM.Frame", %"SAM::__iter__.generator[SAMRecord].SAM.Frame"* %FramePtr, i64 0, i32 3
  %index = load i2, i2* %index.addr, align 1
  %switch = icmp eq i2 %index, 0
  br i1 %switch, label %AfterCoroSuspend, label %for

AfterCoroSuspend:                                 ; preds = %entry.resume
  %.reload.addr = getelementptr inbounds %"SAM::__iter__.generator[SAMRecord].SAM.Frame", %"SAM::__iter__.generator[SAMRecord].SAM.Frame"* %FramePtr, i64 0, i32 4
  %.reload = load i8*, i8** %.reload.addr, align 8
  %0 = call i8* @"SAM::_iter.generator[ptr[byte]].SAM"(i8* %.reload)
  %.spill.addr12 = getelementptr inbounds %"SAM::__iter__.generator[SAMRecord].SAM.Frame", %"SAM::__iter__.generator[SAMRecord].SAM.Frame"* %FramePtr, i64 0, i32 5
  store i8* %0, i8** %.spill.addr12, align 8
  %1 = bitcast i8* %0 to void (i8*)**
  %2 = load void (i8*)*, void (i8*)** %1, align 8
  call fastcc void %2(i8* %0)
  %3 = bitcast i8* %0 to i8**
  %4 = load i8*, i8** %3, align 8
  %5 = icmp eq i8* %4, null
  br i1 %5, label %cleanup1, label %AfterCoroSuspend7

for:                                              ; preds = %entry.resume
  %.reload.addr23 = getelementptr inbounds %"SAM::__iter__.generator[SAMRecord].SAM.Frame", %"SAM::__iter__.generator[SAMRecord].SAM.Frame"* %FramePtr, i64 0, i32 5
  %.reload24 = load i8*, i8** %.reload.addr23, align 8
  %6 = bitcast i8* %.reload24 to i8**
  %7 = bitcast i8* %.reload24 to void (i8*)**
  %8 = load void (i8*)*, void (i8*)** %7, align 8
  call fastcc void %8(i8* nonnull %.reload24)
  %9 = load i8*, i8** %6, align 8
  %10 = icmp eq i8* %9, null
  br i1 %10, label %cleanup1, label %AfterCoroSuspend7

AfterCoroSuspend7:                                ; preds = %for, %AfterCoroSuspend
  %.reload.addr13.pre-phi = phi i8** [ %.spill.addr12, %AfterCoroSuspend ], [ %.reload.addr23, %for ]
  %.reload14 = load i8*, i8** %.reload.addr13.pre-phi, align 8
  %11 = getelementptr inbounds i8, i8* %.reload14, i64 16
  %12 = bitcast i8* %11 to i8**
  %13 = load i8*, i8** %12, align 8
  %14 = call i8* @seq_alloc(i64 104)
  call void @"SAMRecord::__init__.void.SAMRecord.ptr[byte]"(i8* %14, i8* %13)
  store i8* %14, i8** %promise, align 8
  store i2 1, i2* %index.addr, align 1
  br label %CoroEnd

cleanup1:                                         ; preds = %for, %AfterCoroSuspend
  %.reload.addr21.pre-phi = phi i8** [ %.reload.addr23, %for ], [ %.spill.addr12, %AfterCoroSuspend ]
  %.reload22 = load i8*, i8** %.reload.addr21.pre-phi, align 8
  %15 = getelementptr inbounds i8, i8* %.reload22, i64 8
  %16 = bitcast i8* %15 to void (i8*)**
  %17 = load void (i8*)*, void (i8*)** %16, align 8
  call fastcc void %17(i8* nonnull %.reload22)
  %ResumeFn.addr = getelementptr inbounds %"SAM::__iter__.generator[SAMRecord].SAM.Frame", %"SAM::__iter__.generator[SAMRecord].SAM.Frame"* %FramePtr, i64 0, i32 0
  store void (%"SAM::__iter__.generator[SAMRecord].SAM.Frame"*)* null, void (%"SAM::__iter__.generator[SAMRecord].SAM.Frame"*)** %ResumeFn.addr, align 8
  br label %CoroEnd

CoroEnd:                                          ; preds = %cleanup1, %AfterCoroSuspend7
  ret void
}

; Function Attrs: noinline norecurse nounwind readnone uwtable
define fastcc void @"SAM::__iter__.generator[SAMRecord].SAM.destroy"(%"SAM::__iter__.generator[SAMRecord].SAM.Frame"* nocapture %FramePtr) #2 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
entry.destroy:
  ret void
}

; Function Attrs: noinline norecurse nounwind readnone uwtable
define fastcc void @"SAM::__iter__.generator[SAMRecord].SAM.cleanup"(%"SAM::__iter__.generator[SAMRecord].SAM.Frame"* nocapture %FramePtr) local_unnamed_addr #2 personality i32 (i32, i32, i64, i8*, i8*)* @seq_personality {
entry.cleanup:
  ret void
}

attributes #0 = { noinline uwtable "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-jump-tables"="false" }
attributes #1 = { argmemonly noinline nounwind readonly uwtable "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-jump-tables"="false" }
attributes #2 = { noinline norecurse nounwind readnone uwtable "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-jump-tables"="false" }
attributes #3 = { noinline nounwind uwtable "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-jump-tables"="false" }
attributes #4 = { inaccessiblememonly noinline nounwind uwtable "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-jump-tables"="false" }
attributes #5 = { argmemonly nounwind }
attributes #6 = { noinline noreturn uwtable "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-jump-tables"="false" }
attributes #7 = { noinline norecurse nounwind uwtable "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-jump-tables"="false" }
attributes #8 = { noinline norecurse nounwind readonly uwtable "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-jump-tables"="false" }
attributes #9 = { noinline noreturn nounwind uwtable "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-jump-tables"="false" }
