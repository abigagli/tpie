// Copyright (c) 1994 Darren Vengroff
//
// File: test_ami_stack.cpp
// Author: Darren Vengroff <darrenv@eecs.umich.edu>
// Created: 12/15/94
//

#include <portability.h>

#include <versions.h>
VERSION(test_ami_stack_cpp,"$Id: test_ami_stack.cpp,v 1.8 2004-08-12 15:15:12 jan Exp $");

#include "app_config.h"        
#include "parse_args.h"

// Get the AMI_stack definition.
#include <ami_stack.h>

// Utitlities for ascii output.
#include <ami_scan_utils.h>

static char def_crf[] = "osc.txt";
static char def_irf[] = "osi.txt";
static char def_frf[] = "osf.txt";

static char *count_results_filename = def_crf;
static char *intermediate_results_filename = def_irf;
static char *final_results_filename = def_frf;

static bool report_results_count = false;
static bool report_results_intermediate = false;
static bool report_results_final = false;

static const char as_opts[] = "C:I:F:cif";
void parse_app_opt(char c, char *optarg)
{
    switch (c) {
        case 'C':
            count_results_filename = optarg;
        case 'c':
            report_results_count = true;
            break;
        case 'I':
            intermediate_results_filename = optarg;
        case 'i':
            report_results_intermediate = true;
            break;
        case 'F':
            final_results_filename = optarg;
        case 'f':
            report_results_final = true;
            break;
    }
}

int main(int argc, char **argv)
{
    AMI_err ae;

    parse_args(argc,argv,as_opts,parse_app_opt);

    if (verbose) {
      cout << "test_size = " << test_size << "." << endl;
      cout << "test_mm_size = " << static_cast<TPIE_OS_OUTPUT_SIZE_T>(test_mm_size) << "." << endl;
      cout << "random_seed = " << random_seed << "." << endl;
    } else {
        cout << test_size << ' ' << static_cast<TPIE_OS_OUTPUT_SIZE_T>(test_mm_size) << ' ' << random_seed;
    }
    
    // Set the amount of main memory:
    MM_manager.set_memory_limit (test_mm_size);

    AMI_stack<double> amis0;

    // Streams for reporting values to ascii streams.
        ofstream *osc;
    ofstream *osi;
    ofstream *osf;
    cxx_ostream_scan<double> *rptc = NULL;
    cxx_ostream_scan<double> *rpti = NULL;
    cxx_ostream_scan<double> *rptf = NULL;
    
    if (report_results_count) {
        osc = new ofstream(count_results_filename);
        rptc = new cxx_ostream_scan<double>(osc);
    }
    
    if (report_results_intermediate) {
        osi = new ofstream(intermediate_results_filename);
        rpti = new cxx_ostream_scan<double>(osi);
    }
    
    if (report_results_final) {
        osf = new ofstream(final_results_filename);
        rptf = new cxx_ostream_scan<double>(osf);
    }
    
    // Push values.
    TPIE_OS_OFFSET ii;
    for (ii = test_size; ii--; ) {
        amis0.push((double)ii+0.01);
    }

    if (verbose) {
      cout << "Pushed the initial sequence of values." << endl;
        cout << "Stream length = " << amis0.stream_len() << endl;
    }
        
    if (report_results_count) {
        ae = AMI_scan((AMI_STREAM<double> *)&amis0, rptc);
    }

    // Pop them all off.

    double *jj;
    
    for (ii = 0; ii < test_size; ii++ ) {
        amis0.pop(&jj);
        if (ii  + 0.01 != *jj) {
            cout << ii  + 0.01 << " != " << *jj << endl;
        }
    }

    if (verbose) {
      cout << "Popped the initial sequence of values." << endl;
        cout << "Stream length = " << amis0.stream_len() << endl;
    }
    
    return 0;
}
