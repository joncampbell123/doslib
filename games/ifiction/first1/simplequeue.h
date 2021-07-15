
template <typename T,const unsigned short QueueSize> class IFESimpleQueue {
public:
	IFESimpleQueue() : head(0), tail(0) { }
	~IFESimpleQueue() { }

	size_t queueSize(void) const {
		return QueueSize;
	}

	void clear(void) {
		head = tail = 0;
	}

	void clearold(void) {
		if (head != 0) {
			unsigned short i;

			for (i=head;i < tail;i++)
				queue[i-head] = queue[i];

			tail -= head;
			head = 0;
		}
	}

	bool add(const T &ev) {
		if (tail == QueueSize)
			clearold();

		if (tail < QueueSize) {
			queue[tail++] = ev;
			return true;
		}

		return false;
	}

	T* get(void) {
		if (head < tail)
			return &queue[head++];

		return NULL;
	}

	unsigned short		head,tail;
	T			queue[QueueSize];
};

