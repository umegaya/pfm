typedef enum 	/**** HTTPRC status codes ****/
{
	HRC_ERROR = -1,				/* An error response from httpXxxx() */

	HRC_CONTINUE = 100,			/* Everything OK, keep going... */
	HRC_SWITCHING_PROTOCOLS,	/* HRC upgrade to TLS/SSL */

	HRC_OK = 200,				/* OPTIONS/GET/HEAD/POST/TRACE command was successful */
	HRC_CREATED,				/* PUT command was successful */
	HRC_ACCEPTED,				/* DELETE command was successful */
	HRC_NOT_AUTHORITATIVE,		/* Information isn't authoritative */
	HRC_NO_CONTENT,				/* Successful command, no new data */
	HRC_RESET_CONTENT,			/* Content was reset/recreated */
	HRC_PARTIAL_CONTENT,		/* Only a partial file was recieved/sent */

	HRC_MULTIPLE_CHOICES = 300,	/* Multiple files match request */
	HRC_MOVED_PERMANENTLY,		/* Document has moved permanently */
	HRC_MOVED_TEMPORARILY,		/* Document has moved temporarily */
	HRC_SEE_OTHER,				/* See this other link... */
	HRC_NOT_MODIFIED,			/* File not modified */
	HRC_USE_PROXY,				/* Must use a proxy to access this URI */

	HRC_BAD_REQUEST = 400,		/* Bad request */
	HRC_UNAUTHORIZED,			/* Unauthorized to access host */
	HRC_PAYMENT_REQUIRED,		/* Payment required */
	HRC_FORBIDDEN,				/* Forbidden to access this URI */
	HRC_NOT_FOUND,				/* URI was not found */
	HRC_METHOD_NOT_ALLOWED,		/* Method is not allowed */
	HRC_NOT_ACCEPTABLE,			/* Not Acceptable */
	HRC_PROXY_AUTHENTICATION,	/* Proxy Authentication is Required */
	HRC_REQUEST_TIMEOUT,		/* Request timed out */
	HRC_CONFLICT,				/* Request is self-conflicting */
	HRC_GONE,					/* Server has gone away */
	HRC_LENGTH_REQUIRED,		/* A content length or encoding is required */
	HRC_PRECONDITION,			/* Precondition failed */
	HRC_REQUEST_TOO_LARGE,		/* Request entity too large */
	HRC_URI_TOO_LONG,			/* URI too long */
	HRC_UNSUPPORTED_MEDIATYPE,	/* The requested media type is unsupported */
	HRC_REQUESTED_RANGE,		/* The requested range is not satisfiable */
	HRC_EXPECTATION_FAILED,		/* The expectation given in an Expect header field was not met */
	HRC_UPGRADE_REQUIRED = 426,	/* Upgrade to SSL/TLS required */

	HRC_SERVER_ERROR = 500,		/* Internal server error */
	HRC_NOT_IMPLEMENTED,		/* Feature not implemented */
	HRC_BAD_GATEWAY,			/* Bad gateway */
	HRC_SERVICE_UNAVAILABLE,	/* Service is unavailable */
	HRC_GATEWAY_TIMEOUT,		/* Gateway connection timed out */
	HRC_NOT_SUPPORTED			/* this version not supported */
} httprc;
