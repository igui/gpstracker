// 
// 
// 

#include "Reset.h"
#include <avr/wdt.h>

void Reset::resetLoop()
{
	wdt_enable(WDTO_15MS);
	while (true)
	{
	}
}
