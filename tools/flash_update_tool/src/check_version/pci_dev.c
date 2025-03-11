/*
 *	The PCI Library -- Example of use (simplistic lister of PCI devices)
 *
 *	Written by Martin Mares and put to public domain. You can do
 *	with it anything you want, but I don't give you any warranty.
 */

#include <stdio.h>

#include "pciutils.h"
#include "lib/pci.h"

#define BAR0 0
#define BAR1 1
#define BAR2 2
#define BAR3 3
#define BAR4 4
#define BAR5 5

int get_pcie_info(long long *pbase, long *psize, int verbose)
{
	struct pci_access *pacc;
	struct pci_dev *dev;
	unsigned int c;
	char namebuf[1024], *name;
	int devcnt = 0;

	pacc = pci_alloc();		/* Get the pci_access structure */
	/* Set all options you want -- here we stick with the defaults */
	pci_init(pacc);		/* Initialize the PCI library */
	pci_scan_bus(pacc);		/* We want to get the list of devices */

	for (dev=pacc->devices; dev; dev=dev->next)	/* Iterate over all devices */
	{
		pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS);	/* Fill in header info we need */
		c = pci_read_byte(dev, PCI_INTERRUPT_PIN);				/* Read config register directly */

		/* Look for Cadence PCIe Device */
		if( (dev->vendor_id == 0x1FE9) && (dev->device_id==0x100) ) {

			/* Print the device info */
			if(verbose)
			printf("%04x:%02x:%02x.%d vendor=%04x device=%04x class=%04x irq=%d (pin %d) base0=%lx",
					dev->domain, dev->bus, dev->dev, dev->func, dev->vendor_id, dev->device_id,
					dev->device_class, dev->irq, c, (long) dev->base_addr[BAR0]);

			/* Look up and print the full name of the device */
			name = pci_lookup_name(pacc, namebuf, sizeof(namebuf), PCI_LOOKUP_DEVICE, dev->vendor_id, dev->device_id);
			if(verbose)
			printf(" (%s)\n", name);


			/* Enable the memory space access from PCIe Link */
			pci_write_byte(dev, PCI_COMMAND, PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);

			/* Get the Base Addr of BAR 0 */
			pbase[devcnt*6+0] = ((long long)dev->base_addr[BAR0] & (~0xF));
			pbase[devcnt*6+1] = ((long long)dev->base_addr[BAR1] & (~0xF));
			pbase[devcnt*6+2] = ((long long)dev->base_addr[BAR2] & (~0xF));
			pbase[devcnt*6+3] = ((long long)dev->base_addr[BAR3] & (~0xF));
			pbase[devcnt*6+4] = ((long long)dev->base_addr[BAR4] & (~0xF));
			pbase[devcnt*6+5] = ((long long)dev->base_addr[BAR5] & (~0xF));
			
			/* Get the size of BAR 0 */
			psize[devcnt*6+0] = (long)dev->size[BAR0];
			psize[devcnt*6+1] = (long)dev->size[BAR1];
			psize[devcnt*6+2] = (long)dev->size[BAR2];
			psize[devcnt*6+3] = (long)dev->size[BAR3];
			psize[devcnt*6+4] = (long)dev->size[BAR4];
			psize[devcnt*6+5] = (long)dev->size[BAR5];

			devcnt++;		
		}

	}

	/* Close everything */
	pci_cleanup(pacc);
	
	if (verbose)
	printf("****Found %d MemryX Devices.****\n\n", devcnt);
	return devcnt;
}
