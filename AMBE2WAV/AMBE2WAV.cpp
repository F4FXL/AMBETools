/*
*   Copyright (C) 2017 by Jonathan Naylor G4KLX
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "AMBE2WAV.h"

#include "AMBEFileReader.h"
#include "WAVFileWriter.h"

#include <cstring>

#if defined(_WIN32) || defined(_WIN64)
char* optarg = NULL;
int optind = 1;

int getopt(int argc, char* const argv[], const char* optstring)
{
	if ((optind >= argc) || (argv[optind][0] != '-') || (argv[optind][0] == 0))
		return -1;

	int opt = argv[optind][1];
	const char *p = strchr(optstring, opt);

	if (p == NULL) {
		return '?';
	}

	if (p[1] == ':') {
		optind++;
		if (optind >= argc)
			return '?';

		optarg = argv[optind];
		optind++;
	}

	return opt;
}
#else
#include <unistd.h>
#endif

int main(int argc, char** argv)
{
	float amplitude = 1.0F;
	std::string signature;
	AMBE_MODE mode = MODE_DSTAR;
	bool fec = true;
	std::string port = "/dev/ttyUSB0";
	unsigned int speed = 230400U;
	bool reset = false;

	int c;
	while ((c = ::getopt(argc, argv, "a:f:g:m:p:rs:")) != -1) {
		switch (c) {
		case 'a':
			amplitude = float(::atof(optarg));
			break;
		case 'f':
			fec = ::atoi(optarg) != 0;
			break;
		case 'g':
			signature = std::string(optarg);
			break;
		case 'm':
			if (::strcmp(optarg, "dstar") == 0)
				mode = MODE_DSTAR;
			else if (::strcmp(optarg, "dmr") == 0)
				mode = MODE_DMR;
			else if (::strcmp(optarg, "ysf") == 0)
				mode = MODE_YSF;
			else if (::strcmp(optarg, "p25") == 0)
				mode = MODE_P25;
			else
				mode = MODE_UNKNOWN;
			break;
		case 'p':
			port = std::string(optarg);
			break;
		case 'r':
			reset = true;
			break;
		case 's':
			speed = (unsigned int)::atoi(optarg);
			break;
		case '?':
			break;
		default:
			fprintf(stderr, "Usage: AMBE2WAV [-a amplitude] [-g <signature>] [-m dstar|dmr|ysf|p25] [-f 0|1] [-p <port>] [-s <speed>] [-r] <input> <output>\n");
			break;
		}
	}

	if (optind > (argc - 2)) {
		fprintf(stderr, "Usage: AMBE2WAV [-a amplitude] [-g <signature>] [-m dstar|dmr|ysf|p25] [-f 0|1] [-p <port>] [-s <speed>] [-r] <input> <output>\n");
		return 1;
	}

	if (mode == MODE_UNKNOWN) {
		::fprintf(stderr, "AMBE2WAV: unknown mode specified\n");
		return 1;
	}

	if (mode == MODE_DSTAR && !fec) {
		::fprintf(stderr, "AMBE2WAV: incompatible mode and FEC settings\n");
		return 1;
	}

	if (mode == MODE_YSF && !fec) {
		::fprintf(stderr, "AMBE2WAV: incompatible mode and FEC settings\n");
		return 1;
	}

	CAMBE2WAV* ambe2wav = new CAMBE2WAV(signature, mode, fec, port, speed, amplitude, reset, std::string(argv[argc - 2]), std::string(argv[argc - 1]));

	int ret = ambe2wav->run();

	delete ambe2wav;

    return ret;
}

CAMBE2WAV::CAMBE2WAV(const std::string& signature, AMBE_MODE mode, bool fec, const std::string& port, unsigned int speed, float amplitude, bool reset, const std::string& input, const std::string& output) :
m_signature(signature),
m_mode(mode),
m_fec(fec),
m_port(port),
m_speed(speed),
m_amplitude(amplitude),
m_reset(reset),
m_input(input),
m_output(output)
{
}

CAMBE2WAV::~CAMBE2WAV()
{
}

int CAMBE2WAV::run()
{
	CAMBEFileReader reader(m_input, m_signature);
	bool ret = reader.open();
	if (!ret)
		return 1;

	CWAVFileWriter writer(m_output, AUDIO_SAMPLE_RATE, 1U, 16U, AUDIO_BLOCK_SIZE);
	ret = writer.open();
	if (!ret) {
		reader.close();
		return 1;
	}

	CDV3000SerialController controller(m_port, m_speed, m_mode, m_fec, m_amplitude, m_reset, &reader, &writer);
	ret = controller.open();
	if (!ret) {
		writer.close();
		reader.close();
		return 1;
	}

	controller.process();

	controller.close();
	writer.close();
	reader.close();

	return 0;
}
