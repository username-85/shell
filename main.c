#include "rc.h"
#include "sh.h"
#include <stdlib.h>

int main(void)
{
	if (run_shell() != SUCCESS)
		exit(EXIT_FAILURE);

	exit(EXIT_SUCCESS);
}

