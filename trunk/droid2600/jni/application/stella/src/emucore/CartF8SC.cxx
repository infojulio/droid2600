//============================================================================
//
//   SSSS    tt          lll  lll       
//  SS  SS   tt           ll   ll        
//  SS     tttttt  eeee   ll   ll   aaaa 
//   SSSS    tt   ee  ee  ll   ll      aa
//      SS   tt   eeeeee  ll   ll   aaaaa  --  "An Atari 2600 VCS Emulator"
//  SS  SS   tt   ee      ll   ll  aa  aa
//   SSSS     ttt  eeeee llll llll  aaaaa
//
// Copyright (c) 1995-2010 by Bradford W. Mott, Stephen Anthony
// and the Stella Team
//
// See the file "License.txt" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//
// $Id: CartF8SC.cxx,v 1.1.1.1 2010-10-26 03:44:58 cvs Exp $
//============================================================================

#include <cassert>
#include <cstring>

#include "System.hxx"
#include "CartF8SC.hxx"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CartridgeF8SC::CartridgeF8SC(const uInt8* image)
{
  // Copy the ROM image into my buffer
  memcpy(myImage, image, 8192);

  // This cart contains 128 bytes extended RAM @ 0x1000
  registerRamArea(0x1000, 128, 0x80, 0x00);

  // Remember startup bank
  myStartBank = 1;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CartridgeF8SC::~CartridgeF8SC()
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeF8SC::reset()
{
  // Initialize RAM with random values
  for(uInt32 i = 0; i < 128; ++i)
    myRAM[i] = mySystem->randGenerator().next();

  // Upon reset we switch to the startup bank
  bank(myStartBank);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeF8SC::install(System& system)
{
  mySystem = &system;
  uInt16 shift = mySystem->pageShift();
  uInt16 mask = mySystem->pageMask();

  // Make sure the system we're being installed in has a page size that'll work
  assert(((0x1080 & mask) == 0) && ((0x1100 & mask) == 0));

  System::PageAccess access;

  // Set the page accessing methods for the hot spots
  access.directPeekBase = 0;
  access.directPokeBase = 0;
  access.device = this;
  access.type = System::PA_READ;
  for(uInt32 i = (0x1FF8 & ~mask); i < 0x2000; i += (1 << shift))
    mySystem->setPageAccess(i >> shift, access);

  // Set the page accessing method for the RAM writing pages
  access.directPeekBase = 0;
  access.device = this;
  access.type = System::PA_WRITE;
  for(uInt32 j = 0x1000; j < 0x1080; j += (1 << shift))
  {
    access.directPokeBase = &myRAM[j & 0x007F];
    mySystem->setPageAccess(j >> shift, access);
  }
 
  // Set the page accessing method for the RAM reading pages
  access.directPokeBase = 0;
  access.device = this;
  access.type = System::PA_READ;
  for(uInt32 k = 0x1080; k < 0x1100; k += (1 << shift))
  {
    access.directPeekBase = &myRAM[k & 0x007F];
    mySystem->setPageAccess(k >> shift, access);
  }

  // Install pages for the startup bank
  bank(myStartBank);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt8 CartridgeF8SC::peek(uInt16 address)
{
  uInt16 peekAddress = address;
  address &= 0x0FFF;

  // Switch banks if necessary
  switch(address)
  {
    case 0x0FF8:
      // Set the current bank to the lower 4k bank
      bank(0);
      break;
  
    case 0x0FF9:
      // Set the current bank to the upper 4k bank
      bank(1);
      break;
  
    default:
      break;
  }

  if(address < 0x0080)  // Write port is at 0xF000 - 0xF080 (128 bytes)
  {
    // Reading from the write port triggers an unwanted write
    uInt8 value = mySystem->getDataBusState(0xFF);

    if(bankLocked())
      return value;
    else
    {
      triggerReadFromWritePort(peekAddress);
      return myRAM[address] = value;
    }
  }  
  else
    return myImage[(myCurrentBank << 12) + address];
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool CartridgeF8SC::poke(uInt16 address, uInt8)
{
  address &= 0x0FFF;

  // Switch banks if necessary
  switch(address)
  {
    case 0x0FF8:
      // Set the current bank to the lower 4k bank
      bank(0);
      break;
  
    case 0x0FF9:
      // Set the current bank to the upper 4k bank
      bank(1);
      break;
  
    default:
      break;
  }

  // NOTE: This does not handle accessing RAM, however, this function
  // should never be called for RAM because of the way page accessing
  // has been setup
  return false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeF8SC::bank(uInt16 bank)
{ 
  if(bankLocked()) return;

  // Remember what bank we're in
  myCurrentBank = bank;
  uInt16 offset = myCurrentBank << 12;
  uInt16 shift = mySystem->pageShift();
  uInt16 mask = mySystem->pageMask();

  // Setup the page access methods for the current bank
  System::PageAccess access;
  access.directPokeBase = 0;
  access.device = this;
  access.type = System::PA_READ;

  // Map ROM image into the system
  for(uInt32 address = 0x1100; address < (0x1FF8U & ~mask);
      address += (1 << shift))
  {
    access.directPeekBase = &myImage[offset + (address & 0x0FFF)];
    mySystem->setPageAccess(address >> shift, access);
  }
  myBankChanged = true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt16 CartridgeF8SC::bank() const
{
  return myCurrentBank;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt16 CartridgeF8SC::bankCount() const
{
  return 2;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool CartridgeF8SC::patch(uInt16 address, uInt8 value)
{
  address &= 0x0FFF;

  if(address < 0x0100)
  {
    // Normally, a write to the read port won't do anything
    // However, the patch command is special in that ignores such
    // cart restrictions
    myRAM[address & 0x007F] = value;
  }
  else
    myImage[(myCurrentBank << 12) + address] = value;

  return myBankChanged = true;
} 

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const uInt8* CartridgeF8SC::getImage(int& size) const
{
  size = 8192;
  return myImage;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool CartridgeF8SC::save(Serializer& out) const
{
//   try
//   {
    out.putString(name());
    out.putInt(myCurrentBank);

    // The 128 bytes of RAM
    out.putInt(128);
    for(uInt32 i = 0; i < 128; ++i)
      out.putByte((char)myRAM[i]);
//   }
//   catch(const char* msg)
//   {
//     cerr << "ERROR: CartridgeF8SC::save" << endl << "  " << msg << endl;
//     return false;
//   }

  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool CartridgeF8SC::load(Serializer& in)
{
//   try
//   {
    if(in.getString() != name())
      return false;

    myCurrentBank = (uInt16) in.getInt();

    uInt32 limit = (uInt32) in.getInt();
    for(uInt32 i = 0; i < limit; ++i)
      myRAM[i] = (uInt8) in.getByte();
//   }
//   catch(const char* msg)
//   {
//     cerr << "ERROR: CartridgeF8SC::load" << endl << "  " << msg << endl;
//     return false;
//   }

  // Remember what bank we were in
  bank(myCurrentBank);

  return true;
}
