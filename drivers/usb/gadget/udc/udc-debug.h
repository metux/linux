#ifndef __UDC_DEBUG_H
#define __UDC_DEBUG_H

/* debug/log helpers for udc device

   * includers can define VERBOSE to enable more verbose debugging
     (udc_vdbg() / ep_vdbg() and USB_TRACE to enable detailed
     trace messages (udc_trace() / ep_trace())
   * includers may also define their own versions of udc_to_dev(),
     ep_to_dev() or ep_to_name(), when having different pointer names
*/

/* individual drivers may override this */
#ifndef udc_to_dev
#define udc_to_dev(ptr)	(&(ptr->pdev->dev))
#endif

#ifndef ep_to_dev
#define ep_to_dev(ptr)	(&(ptr->udc->pdev->dev))
#endif

#ifndef ep_to_name
#define ep_to_name(ptr)	(ptr->ep.name)
#endif


/* logging on udc device:

     * dev needs to point to one of the *_udc structs found in several drivers
     * this struct needs to have an struct platform_device *pdev field.
     * will call the corresponding dev_*() loggig functions
*/
#define udc_err(udc, fmt, ...) \
	dev_err(udc_to_dev(udc), fmt, ## __VA_ARGS__);

#define udc_warn(udc, fmt, ...) \
	dev_warn(udc_to_dev(udc), fmt, ## __VA_ARGS__);

#define udc_info(udc, fmt, ...) \
	dev_info(udc_to_dev(udc), fmt, ## __VA_ARGS__);

#define udc_dbg(udc, fmt, ...) \
	dev_dbg(udc_to_dev(udc), fmt, ## __VA_ARGS__);

/* logging on endpoints:

   ep_*(ep, ...): logging on endpoint
    * ep needs to point to one of the *_ep structs found in several drivers
    * this struct needs to have an pointer to the *_udc struct, called "udc"
    * will call dev_*() and add prefix with endpoint name
*/
#define ep_err(_ep, fmt, ...) \
	dev_err(ep_to_dev(_ep), "EP %s: " fmt, ep_to_name(_ep), ## __VA_ARGS__);

#define ep_warn(ep, fmt, ...) \
	dev_warn(ep_to_dev(_ep), "EP %s: " fmt, ep_to_name(_ep), ## __VA_ARGS__);

#define ep_info(_ep, fmt, ...) \
	dev_info(ep_to_dev(_ep), "EP %s: " fmt, ep_to_name(_ep), ## __VA_ARGS__);

#define ep_dbg(_ep, fmt, ...) \
	dev_dbg(ep_to_dev(_ep), "EP %s: " fmt, ep_to_name(_ep), ## __VA_ARGS__);

#ifdef VERBOSE_DEBUG
#define udc_vdbg udc_dbg
#define ep_vdbg ep_dbg
#else
#define udc_vdbg(udc, fmt, ...)
#define ep_vdbg(ep, fmt, ...)
#endif

#ifdef PACKET_TRACE
#define udc_trace udc_dbg
#define ep_trace ep_dbg
#else
#define udc_trace(udc, fmt, ...)
#define ep_trace(ep, fmt, ...)
#endif

#endif /* __UDC_DEBUG_H */
