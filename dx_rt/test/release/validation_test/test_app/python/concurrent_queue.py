"""
Concurrent queue implementation for thread-safe operations
Ported from C++ version
"""

import threading
import queue
from typing import TypeVar, Generic, Optional

T = TypeVar('T')

class ConcurrentQueue(Generic[T]):
    """Thread-safe queue implementation"""
    
    def __init__(self, max_size: int = 0):
        """
        Initialize concurrent queue
        
        Args:
            max_size: Maximum size of queue (0 for unlimited)
        """
        if max_size <= 0:
            self._queue = queue.Queue()
        else:
            self._queue = queue.Queue(maxsize=max_size)
        
        self._mutex = threading.Lock()

    def put(self, item: T, block: bool = True, timeout: Optional[float] = None) -> bool:
        """
        Put an item into the queue
        
        Args:
            item: Item to put in queue
            block: Whether to block if queue is full
            timeout: Timeout for blocking operation
            
        Returns:
            True if item was successfully added, False otherwise
        """
        try:
            self._queue.put(item, block=block, timeout=timeout)
            return True
        except queue.Full:
            return False

    def get(self, block: bool = True, timeout: Optional[float] = None) -> Optional[T]:
        """
        Get an item from the queue
        
        Args:
            block: Whether to block if queue is empty
            timeout: Timeout for blocking operation
            
        Returns:
            Item from queue or None if unavailable
        """
        try:
            return self._queue.get(block=block, timeout=timeout)
        except queue.Empty:
            return None

    def try_put(self, item: T) -> bool:
        """
        Try to put an item without blocking
        
        Args:
            item: Item to put in queue
            
        Returns:
            True if successful, False if queue is full
        """
        return self.put(item, block=False)

    def try_get(self) -> Optional[T]:
        """
        Try to get an item without blocking
        
        Returns:
            Item from queue or None if empty
        """
        return self.get(block=False)

    def empty(self) -> bool:
        """
        Check if queue is empty
        
        Returns:
            True if queue is empty, False otherwise
        """
        return self._queue.empty()

    def size(self) -> int:
        """
        Get current queue size
        
        Returns:
            Number of items in queue
        """
        return self._queue.qsize()

    def full(self) -> bool:
        """
        Check if queue is full
        
        Returns:
            True if queue is full, False otherwise
        """
        return self._queue.full()

    def clear(self):
        """Clear all items from the queue"""
        with self._mutex:
            while not self._queue.empty():
                try:
                    self._queue.get_nowait()
                except queue.Empty:
                    break

    def __len__(self) -> int:
        """Get queue size using len() function"""
        return self.size()

    def __bool__(self) -> bool:
        """Check if queue has items using bool() function"""
        return not self.empty()