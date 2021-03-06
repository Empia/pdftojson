//========================================================================
//
// pdftojson.cc
//
// Copyright 2005 Glyph & Cog, LLC
// Modified by Laurent Denoue to make pdftohtml.cc export JSON
//
//========================================================================

#include <aconf.h>
#include <stdio.h>
#include <stdlib.h>
#include "parseargs.h"
#include "gmem.h"
#include "gfile.h"
#include "GString.h"
#include "GlobalParams.h"
#include "PDFDoc.h"
#include "JSONGen.h"
#include "Error.h"
#include "ErrorCodes.h"
#include "config.h"

//------------------------------------------------------------------------

static int firstPage = 1;
static int lastPage = 0;
static int resolution = 150;
static GBool skipInvisible = gFalse;
static char ownerPassword[33] = "\001";
static char userPassword[33] = "\001";
static GBool quiet = gFalse;
static char cfgFileName[256] = "";
static GBool printVersion = gFalse;
static GBool printHelp = gFalse;

static ArgDesc argDesc[] = {
  {"-f",       argInt,      &firstPage,     0,
   "first page to convert"},
  {"-l",       argInt,      &lastPage,      0,
   "last page to convert"},
  {"-r",      argInt,      &resolution,     0,
   "resolution, in DPI (default is 150)"},
  {"-skipinvisible", argFlag, &skipInvisible, 0,
   "do not draw invisible text"},
  {"-opw",     argString,   ownerPassword,  sizeof(ownerPassword),
   "owner password (for encrypted files)"},
  {"-upw",     argString,   userPassword,   sizeof(userPassword),
   "user password (for encrypted files)"},
  {"-q",       argFlag,     &quiet,         0,
   "don't print any messages or errors"},
  {"-cfg",     argString,   cfgFileName,    sizeof(cfgFileName),
   "configuration file to use in place of .xpdfrc"},
  {"-v",       argFlag,     &printVersion,  0,
   "print copyright and version info"},
  {"-h",       argFlag,     &printHelp,     0,
   "print usage information"},
  {"-help",    argFlag,     &printHelp,     0,
   "print usage information"},
  {"--help",   argFlag,     &printHelp,     0,
   "print usage information"},
  {"-?",       argFlag,     &printHelp,     0,
   "print usage information"},
  {NULL}
};

//------------------------------------------------------------------------

static int writeToFile(void *file, const char *data, int size) {
  return (int)fwrite(data, 1, size, (FILE *)file);
}

int main(int argc, char *argv[]) {
    PDFDoc *doc;
    GString *fileName;
    GString *jsonFilename;
    GString *ownerPW, *userPW;
    JSONGen *jsonGen;
    GString *htmlFileName, *pngFileName, *pngURL;
    FILE *jsonFile, *pngFile;
    int pg, err, exitCode;
    GBool ok;
    
    exitCode = 99;
    
    // parse args
    ok = parseArgs(argDesc, &argc, argv);
    if (!ok || argc != 3 || printVersion || printHelp) {
        fprintf(stderr, "pdftojson version %s\n", xpdfVersion);
        fprintf(stderr, "%s\n", xpdfCopyright);
        if (!printVersion) {
            printUsage("pdftojson", "<PDF-file> <JSON-file>", argDesc);
        }
        goto err0;
    }
    fileName = new GString(argv[1]);
    jsonFilename = new GString(argv[2]);
    
    // read config file
    globalParams = new GlobalParams(cfgFileName);
    if (quiet) {
        globalParams->setErrQuiet(quiet);
    }
    globalParams->setupBaseFonts(NULL);
    globalParams->setTextEncoding("UTF-8");
    
    // open PDF file
    if (ownerPassword[0] != '\001') {
        ownerPW = new GString(ownerPassword);
    } else {
        ownerPW = NULL;
    }
    if (userPassword[0] != '\001') {
        userPW = new GString(userPassword);
    } else {
        userPW = NULL;
    }
    doc = new PDFDoc(fileName, ownerPW, userPW);
    if (userPW) {
        delete userPW;
    }
    if (ownerPW) {
        delete ownerPW;
    }
    if (!doc->isOk()) {
        exitCode = 1;
        goto err1;
    }
    
    // check for copy permission
    // Laurent removed this
    /*if (!doc->okToCopy()) {
     error(errNotAllowed, -1,
     "Copying of text from this document is not allowed.");
     exitCode = 3;
     goto err1;
     }*/
    
    // get page range
    if (firstPage < 1) {
        firstPage = 1;
    }
    if (lastPage < 1 || lastPage > doc->getNumPages()) {
        lastPage = doc->getNumPages();
    }
    
    // set up the JSONGen object
    jsonGen = new JSONGen(resolution);
    if (!jsonGen->isOk()) {
        exitCode = 99;
        goto err1;
    }
    jsonGen->setDrawInvisibleText(!skipInvisible);
    jsonGen->startDoc(doc);
    
    if (!(jsonFile = fopen(jsonFilename->getCString(), "wb"))) {
        error(errIO, -1, "Couldn't open JSON file '{0:t}'", jsonFilename);
        delete jsonFilename;
        delete pngFileName;
        goto err2;
    }
    fprintf(jsonFile,"[");
    // convert the pages
    for (pg = firstPage; pg <= lastPage; ++pg) {
        
        err = jsonGen->convertPage(pg, &writeToFile, jsonFile);
        if (pg < lastPage)
            fprintf(jsonFile,",");
        if (err != errNone) {
            error(errIO, -1, "Error converting page {0:d}", pg);
            exitCode = 2;
            goto err2;
        }
    }
    fprintf(jsonFile,"]");
    fclose(jsonFile);
    exitCode = 0;
    
    // clean up
err2:
    delete jsonGen;
err1:
    delete doc;
    delete globalParams;
err0:
    
    // check for memory leaks
    Object::memCheck(stderr);
    gMemReport(stderr);
    
    return exitCode;
}
