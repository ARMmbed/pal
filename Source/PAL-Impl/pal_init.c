/*
* Copyright (c) 2016 ARM Limited. All rights reserved.
* SPDX-License-Identifier: Apache-2.0
* Licensed under the Apache License, Version 2.0 (the License); you may
* not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an AS IS BASIS, WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/


#include "pal.h"
#include "pal_plat_rtos.h"
#include "pal_plat_network.h"
#include "pal_macros.h"

int g_palIntialized = 0;


palStatus_t pal_init()
{

	palStatus_t status = PAL_SUCCESS;
	
	if (1 <= g_palIntialized)
	{
		PAL_MODULE_INIT(g_palIntialized);
		return PAL_SUCCESS;
	}

	status = pal_plat_RTOSInitialize(NULL);
	if (PAL_SUCCESS != status)
	{
		pal_plat_RTOSDestroy();
	}

	else 
	{
		status = pal_plat_sockets_init(NULL);
	}

	if (PAL_SUCCESS == status)
	{
		PAL_MODULE_INIT(g_palIntialized);
	}

	return status;
}


void pal_destroy()
{
	PAL_MODULE_DEINIT(g_palIntialized);
	if(0 == g_palIntialized)
	{
		pal_plat_RTOSDestroy();
		pal_plat_sockets_terminate(NULL);
	}
}
