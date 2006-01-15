 /* BonkEnc Audio Encoder
  * Copyright (C) 2001-2006 Robert Kausch <robert.kausch@bonkenc.org>
  *
  * This program is free software; you can redistribute it and/or
  * modify it under the terms of the "GNU General Public License".
  *
  * THIS PACKAGE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
  * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. */

#include <input/filter-in-au.h>

BonkEnc::FilterInAU::FilterInAU(Config *config, Track *format) : InputFilter(config, format)
{
	packageSize = 0;
}

BonkEnc::FilterInAU::~FilterInAU()
{
}

Bool BonkEnc::FilterInAU::Activate()
{
	InStream	*in = new InStream(STREAM_DRIVER, driver);
    
	in->InputNumber(4); // Read magic number

	Int		 headerSize = in->InputNumberRaw(4);

	delete in;

	driver->Seek(headerSize);

	return true;
}

Bool BonkEnc::FilterInAU::Deactivate()
{
	return true;
}

Int BonkEnc::FilterInAU::ReadData(UnsignedByte **data, Int size)
{
	buffer.Resize(size);

	driver->ReadData(buffer, size);

	*data = buffer;

	return size;
}

BonkEnc::Track *BonkEnc::FilterInAU::GetFileInfo(const String &inFile)
{
	Track		*nFormat = new Track;
	InStream	*f_in = new InStream(STREAM_FILE, inFile, IS_READONLY);

	// TODO: Add more checking to this!

	nFormat->fileSize = f_in->Size();
	nFormat->order = BYTE_RAW;

	// Read magic number and header size
	for (Int i = 0; i < 8; i++)
		f_in->InputNumber(1);

	nFormat->length = uint32(f_in->InputNumberRaw(4));
	nFormat->bits = uint32(f_in->InputNumberRaw(4));

	if (nFormat->bits == 3)		nFormat->bits = 16;
	else if (nFormat->bits == 2)	nFormat->bits = 8;

	nFormat->length = nFormat->length / (nFormat->bits / 8);

	nFormat->rate = uint32(f_in->InputNumberRaw(4));
	nFormat->channels = uint32(f_in->InputNumberRaw(4));

	delete f_in;

	return nFormat;
}
