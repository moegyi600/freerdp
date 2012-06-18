/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Print Virtual Channel - Ulteo Open Virtual Desktop PDF printer
 *
 *
 * Copyright 2012 Ulteo SAS http://www.ulteo.com
 *    Author: Jocelyn DELALANDE <j.delalande@ulteo.com>
 *
 * Inspired by printer_cups.h - Copyright 2010-2011 Vic Lee
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/svc_plugin.h>

#include "rdpdr_constants.h"
#include "rdpdr_types.h"
#include "printer_main.h"

#include "printer_ulteo_pdf.h"

typedef struct rdp_ulteo_pdf_printer_driver rdpUlteoPdfPrinterDriver;
typedef struct rdp_ulteo_pdf_printer rdpUlteoPdfPrinter;
typedef struct rdp_ulteo_pdf_print_job rdpUlteoPdfPrintJob;

struct rdp_ulteo_pdf_printer_driver
{
	rdpPrinterDriver driver;

	int id_sequence;
};

struct rdp_ulteo_pdf_printer
{
	rdpPrinter printer;

	rdpPrintJob* printjob;
};

static void printer_ulteo_pdf_get_printjob_name(char* buf, int size)
{
	time_t tt;
	struct tm* t;

	tt = time(NULL);
	t = localtime(&tt);
	snprintf(buf, size - 1, "FreeRDP Print Job %d%02d%02d%02d%02d%02d",
		t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
		t->tm_hour, t->tm_min, t->tm_sec);
}


/// Here we have in data the PDF data as received from the server.
static void printer_ulteo_pdf_write_printjob(rdpPrintJob* printjob, uint8* data, int size)
{
	DEBUG_WARN("Write %i job data", size);
}


/// Simply called when the document has finished transmission
static void printer_ulteo_pdf_close_printjob(rdpPrintJob* printjob)
{
	DEBUG_WARN("Close %i job", printjob->id);
	((rdpUlteoPdfPrinter*)printjob->printer)->printjob = NULL;
}

static rdpPrintJob* printer_ulteo_pdf_create_printjob(rdpPrinter* printer, uint32 id)
{
	rdpUlteoPdfPrinter* ulteo_pdf_printer = (rdpUlteoPdfPrinter*)printer;
	rdpPrintJob* printjob = xnew(rdpPrintJob);

	DEBUG_WARN("Create %i job", printjob->id);

	// Means that we cannot transmit two documents at the same time on the remote printer.
	if (ulteo_pdf_printer->printjob != NULL) {
		DEBUG_WARN("Refusing job %i as another is still transmissing", 
				   id);
		return NULL;
	}

	printjob->id = id;
	printjob->printer = printer;

	printjob->Write = printer_ulteo_pdf_write_printjob;
	printjob->Close = printer_ulteo_pdf_close_printjob;

	ulteo_pdf_printer->printjob = printjob;

	//TODO: keep track of the jobs in a list.

	return printjob;
}

static rdpPrintJob* printer_ulteo_pdf_find_printjob(rdpPrinter* printer, uint32 id)
{
	rdpUlteoPdfPrinter* ulteo_pdf_printer = (rdpUlteoPdfPrinter*)printer;

	if ((ulteo_pdf_printer->printjob == NULL) ||  (ulteo_pdf_printer->printjob->id != id))
		return NULL;

	return (rdpPrintJob*)ulteo_pdf_printer->printjob;
}

static void printer_ulteo_pdf_free_printer(rdpPrinter* printer)
{
	rdpUlteoPdfPrinter* ulteo_pdf_printer = (rdpUlteoPdfPrinter*)printer;

	if (ulteo_pdf_printer->printjob)
		ulteo_pdf_printer->printjob->Close((rdpPrintJob*)ulteo_pdf_printer->printjob);
	xfree(printer->name);
	xfree(printer);
}

static rdpPrinter* printer_ulteo_pdf_new_printer(rdpUlteoPdfPrinterDriver* ulteo_pdf_driver, const char* name, boolean is_default)
{
	rdpUlteoPdfPrinter* ulteo_pdf_printer;

	ulteo_pdf_printer = xnew(rdpUlteoPdfPrinter);

	ulteo_pdf_printer->printer.id = ulteo_pdf_driver->id_sequence++;
	ulteo_pdf_printer->printer.name = xstrdup(name);
	/* This is the PDF Ulteo driver. On printing, the printer transmits a PDF back to the client. */
	ulteo_pdf_printer->printer.driver = "Ulteo TS Printer Driver";
	ulteo_pdf_printer->printer.is_default = is_default;

	ulteo_pdf_printer->printer.CreatePrintJob = printer_ulteo_pdf_create_printjob;
	ulteo_pdf_printer->printer.FindPrintJob = printer_ulteo_pdf_find_printjob;
	ulteo_pdf_printer->printer.Free = printer_ulteo_pdf_free_printer;

	return (rdpPrinter*)ulteo_pdf_printer;
}

static rdpPrinter** printer_ulteo_pdf_enum_printers(rdpPrinterDriver* driver)
{
	rdpPrinter** printers;
	
	printers = (rdpPrinter**)xzalloc(sizeof(rdpPrinter*) * 2);
	printers[0] = printer_ulteo_pdf_new_printer((rdpUlteoPdfPrinterDriver*)driver,
												"Ulteo OVD Printer",true);
	return printers;
}

static rdpPrinter* printer_ulteo_pdf_get_printer(rdpPrinterDriver* driver, const char* name)
{
	rdpUlteoPdfPrinterDriver* ulteo_pdf_driver = (rdpUlteoPdfPrinterDriver*)driver;

	return printer_ulteo_pdf_new_printer(ulteo_pdf_driver, name, ulteo_pdf_driver->id_sequence == 1 ? true : false);
}

// Single-instance (singleton-style)
// It's not a printer driver but a printer-manager driver.
static rdpUlteoPdfPrinterDriver* ulteo_pdf_driver = NULL;

rdpPrinterDriver* printer_ulteo_pdf_get_driver(void)
{
	if (ulteo_pdf_driver == NULL)
	{
		ulteo_pdf_driver = xnew(rdpUlteoPdfPrinterDriver);

		ulteo_pdf_driver->driver.EnumPrinters = printer_ulteo_pdf_enum_printers;
		ulteo_pdf_driver->driver.GetPrinter = printer_ulteo_pdf_get_printer;

		ulteo_pdf_driver->id_sequence = 1;

	}

	return (rdpPrinterDriver*)ulteo_pdf_driver;
}

