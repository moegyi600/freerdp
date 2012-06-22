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
#include <unistd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

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
	int spool_fifo;
	char spool_dir[MAX_PATH_SIZE];
};

/* static void printer_ulteo_pdf_get_printjob_name(char* buf, int size) */
/* { */
/* 	time_t tt; */
/* 	struct tm* t; */

/* 	tt = time(NULL); */
/* 	t = localtime(&tt); */
/* 	snprintf(buf, size - 1, "FreeRDP Print Job %d%02d%02d%02d%02d%02d", */
/* 		t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, */
/* 		t->tm_hour, t->tm_min, t->tm_sec); */
/* } */


/**
 * Return the name of the job filename
 *
 * The char* returned must be freed by the caller.
 */
char* printer_ulteo_pdf_get_job_filename(const rdpPrintJob* printJob) {
	rdpUlteoPdfPrinter* printer = (rdpUlteoPdfPrinter*)printJob->printer;
	char* filename = malloc(sizeof(char)*MAX_PATH_SIZE);

	sprintf(filename,"%s/%d.pdf", printer->spool_dir, printJob->id);
	return filename;
}


/// Should be the same FIFO filename as in libguac-client-rdp

//FIXME: Trashit
/* char* printer_ulteo_pdf_get_fifo_filename() { */
/* 	char* filename = malloc(sizeof(char)*200); */
/* 	sprintf(filename,"%s/fifo", FREERDP_ULTEO_SPOOL_PATH); */
/* 	return filename; */
/* } */

/// Here we have in data the PDF data as received from the server.
static void printer_ulteo_pdf_write_printjob(rdpPrintJob* printjob, uint8* data, int size)
{
	FILE* fp;

	// Path of the PDF
	char* filename = printer_ulteo_pdf_get_job_filename(printjob);

	fp = fopen(filename, "a+b");
	if (fp == NULL)
		{
			DEBUG_WARN("failed to open file %s", filename);
			return;
		}
	if (fwrite(data, 1, size, fp) < size)
		{
			DEBUG_WARN("failed to write file %s", filename);
		}
	fclose(fp);
	DEBUG_WARN("Write %i job data", size);

	// Now notify guacd about this file being ready.
	//write(printer->spool_fifo, filename, strlen(filename)+1);

	free(filename);

}


/**
 * Simply called when the document has finished transmission
 *
 * Transmits the information to guacd that a PDF job is ready.
 */
static void printer_ulteo_pdf_close_printjob(rdpPrintJob* printjob)
{
	char* job_filename = printer_ulteo_pdf_get_job_filename(printjob);
	rdpUlteoPdfPrinter* printer = (rdpUlteoPdfPrinter*)(printjob->printer);

	DEBUG_WARN("Close %i job, wrote to %s", printjob->id, job_filename);
	//char* fifo_filename = printer_ulteo_pdf_get_fifo_filename();

	// Communicate with guacd
	if (write(printer->spool_fifo, job_filename, strlen(job_filename)+1) <= 0) {
		DEBUG_WARN("Cannot write to fifo in %s", printer->spool_dir);
		perror("reason :");
	}

	free(job_filename);
	((rdpUlteoPdfPrinter*)printjob->printer)->printjob = NULL;
}

static rdpPrintJob* printer_ulteo_pdf_create_printjob(rdpPrinter* printer, uint32 id)
{
	rdpUlteoPdfPrinter* ulteo_pdf_printer = (rdpUlteoPdfPrinter*)printer;
	rdpPrintJob* printjob = xnew(rdpPrintJob);


	if (access(ulteo_pdf_printer->spool_dir, W_OK) != 0) {
		printf("Error , %s : cannot open to write PDF files", ulteo_pdf_printer->spool_dir);
	}

	/* free(filename); */
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

	// spool is set later in extrainit()
	ulteo_pdf_printer->spool_fifo = -1;

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

	return (rdpPrinter*)(printer_ulteo_pdf_new_printer(ulteo_pdf_driver, name, ulteo_pdf_driver->id_sequence == 1 ? true : false));
}

static int printer_ulteo_pdf_extra_init(char** plugin_data, rdpPrinter* rdp_printer) {
	rdpUlteoPdfPrinter* printer = (rdpUlteoPdfPrinter*)rdp_printer;

	// Guacd stores the path of the spool here.
	char* fifo_path = plugin_data[3];

    printer->spool_fifo = open(fifo_path, O_WRONLY);
    if (printer->spool_fifo <= 0) {
		printf("Cannot open FIFO %s\n", fifo_path);
        perror("reason : ");
        return -3;
    }

	strcpy(printer->spool_dir, fifo_path);

	// Truncate the path removing "/fifo" 
	*(rindex(printer->spool_dir, '/')) = '\0';
	DEBUG_WARN("SPool dir is %s", printer->spool_dir);

	if (access(printer->spool_dir, W_OK) != 0) {
		printf("Error , %s : cannot open to write PDF files", printer->spool_dir);
	}

	return 0;
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
		ulteo_pdf_driver->driver.ExtraInit = printer_ulteo_pdf_extra_init;

		ulteo_pdf_driver->id_sequence = 1;

	}

	return (rdpPrinterDriver*)ulteo_pdf_driver;
}

