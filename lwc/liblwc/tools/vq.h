#ifndef __LWC_VQ_H__
#define __LWC_VQ_H__

#define Q_NEW_HEAD(Q_HEAD_TYPE, Q_ELEM_TYPE) \
	typedef struct{ \
		struct Q_ELEM_TYPE* head; \
		struct Q_ELEM_TYPE* last; \
	} Q_HEAD_TYPE;

#define Q_NEW_LINK(Q_ELEM_TYPE) struct { struct Q_ELEM_TYPE* next; struct Q_ELEM_TYPE* prev; }

#define Q_INIT_HEAD(Q_HEAD) {\
	 (Q_HEAD)->head = NULL;\
	 (Q_HEAD)->last = NULL;\
 }

#define Q_INIT_ELEM(Q_ELEM, LINK_NAME) {\
	(Q_ELEM)->LINK_NAME.next = NULL;\
	(Q_ELEM)->LINK_NAME.prev = NULL;\
}

#define Q_INSERT_FRONT(Q_HEAD, Q_ELEM, LINK_NAME) {\
	(Q_ELEM)->LINK_NAME.next = (Q_HEAD)->head;\
	(Q_ELEM)->LINK_NAME.prev = NULL;\
	if((Q_HEAD)->last == NULL){\
		(Q_HEAD)->last = (Q_ELEM);\
	}else{\
		(Q_HEAD)->head->LINK_NAME.prev = (Q_ELEM);\
	}\
	(Q_HEAD)->head = (Q_ELEM);\
}

#define Q_INSERT_TAIL(Q_HEAD, Q_ELEM, LINK_NAME) {\
	(Q_ELEM)->LINK_NAME.prev = (Q_HEAD)->last;\
	(Q_ELEM)->LINK_NAME.next = NULL;\
	if((Q_HEAD)->head == NULL){\
		(Q_HEAD)->head = (Q_ELEM);\
	}else{\
		(Q_HEAD)->last->LINK_NAME.next = (Q_ELEM);\
	}\
	(Q_HEAD)->last = (Q_ELEM);\
}

#define Q_GET_FRONT(Q_HEAD) (Q_HEAD)->head

#define Q_GET_TAIL(Q_HEAD) (Q_HEAD)->last

#define Q_GET_NEXT(Q_ELEM, LINK_NAME) (Q_ELEM)->LINK_NAME.next

#define Q_GET_PREV(Q_ELEM, LINK_NAME) (Q_ELEM)->LINK_NAME.prev


#define Q_INSERT_AFTER(Q_HEAD,Q_INQ,Q_TOINSERT,LINK_NAME) {\
	if((Q_HEAD)->last == (Q_INQ)){\
		Q_INSERT_TAIL(Q_HEAD, Q_TOINSERT, LINK_NAME);\
	}else{\
		(Q_TOINSERT)->LINK_NAME.prev = (Q_INQ);\
		(Q_TOINSERT)->LINK_NAME.next = (Q_INQ)->LINK_NAME.next;\
		(Q_INQ)->LINK_NAME.next->LINK_NAME.prev = (Q_TOINSERT);\
		(Q_INQ)->LINK_NAME.next = (Q_TOINSERT);\
	}\
}

#define Q_INSERT_BEFORE(Q_HEAD,Q_INQ,Q_TOINSERT,LINK_NAME) {\
	if((Q_HEAD)->head == (Q_INQ)){\
		Q_INSERT_FRONT(Q_HEAD, Q_TOINSERT, LINK_NAME);\
	}else{\
		(Q_TOINSERT)->LINK_NAME.next = (Q_INQ);\
		(Q_TOINSERT)->LINK_NAME.prev = (Q_INQ)->LINK_NAME.prev;\
		(Q_INQ)->LINK_NAME.prev->LINK_NAME.next = (Q_TOINSERT);\
		(Q_INQ)->LINK_NAME.prev = (Q_TOINSERT);\
	}\
}

#define Q_REMOVE(Q_HEAD,Q_ELEM,LINK_NAME) {\
	if((Q_ELEM) == (Q_HEAD)->head && (Q_ELEM) == (Q_HEAD)->last) {\
		(Q_HEAD)->last = NULL;\
		(Q_HEAD)->head = NULL;\
	}else if((Q_ELEM) == (Q_HEAD)->head) {\
		(Q_HEAD)->head = (Q_ELEM)->LINK_NAME.next;\
		(Q_HEAD)->head->LINK_NAME.prev = NULL;\
	}else if((Q_ELEM) == (Q_HEAD)->last) {\
		(Q_HEAD)->last = (Q_ELEM)->LINK_NAME.prev;\
		(Q_HEAD)->last->LINK_NAME.next = NULL;\
	}else {\
		(Q_ELEM)->LINK_NAME.prev->LINK_NAME.next = (Q_ELEM)->LINK_NAME.next;\
		(Q_ELEM)->LINK_NAME.next->LINK_NAME.prev = (Q_ELEM)->LINK_NAME.prev;\
	}\
	(Q_ELEM)->LINK_NAME.prev = NULL;\
	(Q_ELEM)->LINK_NAME.next = NULL;\
}

#define Q_FOREACH(CURRENT_ELEM,Q_HEAD,LINK_NAME) \
	for(CURRENT_ELEM = (Q_HEAD)->head;\
		CURRENT_ELEM != NULL;\
		CURRENT_ELEM = CURRENT_ELEM->LINK_NAME.next)

#endif	/*__LWC_VQ_H__*/