import React, { useState, useRef, useCallback } from 'react';
import AtomicALUSurface from './Atomic ALU Surface.tsx'

interface SwipeContainerProps {
  children: React.ReactNode;
  onSwipeLeft?: () => void;
  onSwipeRight?: () => void;
  onSwipeUp?: () => void;
  onSwipeDown?: () => void;
  threshold?: number;
  className?: string;
}

interface TouchPosition {
  x: number;
  y: number;
}

const SwipeContainer: React.FC<SwipeContainerProps> = ({
  children,
  onSwipeLeft,
  onSwipeRight,
  onSwipeUp,
  onSwipeDown,
  threshold = 50,
  className = ''
}) => {
  const [startPos, setStartPos] = useState<TouchPosition | null>(null);
  const [isDragging, setIsDragging] = useState(false);
  const containerRef = useRef<HTMLDivElement>(null);

  const getPosition = useCallback((e: React.TouchEvent | React.MouseEvent): TouchPosition => {
    if ('touches' in e) {
      return {
        x: e.touches[0].clientX,
        y: e.touches[0].clientY
      };
    }
    return {
      x: e.clientX,
      y: e.clientY
    };
  }, []);

  const handleStart = useCallback((e: React.TouchEvent | React.MouseEvent) => {
    const pos = getPosition(e);
    setStartPos(pos);
    setIsDragging(true);
  }, [getPosition]);

  const handleEnd = useCallback((e: React.TouchEvent | React.MouseEvent) => {
    if (!startPos || !isDragging) return;

    const endPos = getPosition(e);
    const deltaX = endPos.x - startPos.x;
    const deltaY = endPos.y - startPos.y;

    // Determine if the swipe was significant enough
    const absX = Math.abs(deltaX);
    const absY = Math.abs(deltaY);

    if (absX > threshold || absY > threshold) {
      // Determine primary direction
      if (absX > absY) {
        // Horizontal swipe
        if (deltaX > 0) {
          onSwipeRight?.();
        } else {
          onSwipeLeft?.();
        }
      } else {
        // Vertical swipe
        if (deltaY > 0) {
          onSwipeDown?.();
        } else {
          onSwipeUp?.();
        }
      }
    }

    setStartPos(null);
    setIsDragging(false);
  }, [startPos, isDragging, threshold, onSwipeLeft, onSwipeRight, onSwipeUp, onSwipeDown, getPosition]);

  const handleMouseLeave = useCallback(() => {
    setStartPos(null);
    setIsDragging(false);
  }, []);

  return (
    <div
      ref={containerRef}
      className={`select-none ${className}`}
      onTouchStart={handleStart}
      onTouchEnd={handleEnd}
      onMouseDown={handleStart}
      onMouseUp={handleEnd}
      onMouseLeave={handleMouseLeave}
      style={{ touchAction: 'none' }}
    >
      {children}
    </div>
  );
};

const defaultCards = [
  { id: 1, color: 'bg-red-400', title: 'Card 1' },
  { id: 2, color: 'bg-blue-400', title: 'Card 2' },
  { id: 3, color: 'bg-green-400', title: 'Card 3' },
  { id: 4, color: 'bg-yellow-400', title: 'Card 4' },
  { id: 5, color: 'bg-purple-400', title: 'Card 5', element: <AtomicALUSurface /> }
];
// Demo component to show the swipe container in action
const SwipeDemo: React.FC<{ cards?: typeof defaultCards }> = ({ cards = defaultCards }) => {
  const [currentCard, setCurrentCard] = useState(0);
  const [lastSwipe, setLastSwipe] = useState<string>('');


  const nextCard = () => {
    setCurrentCard((prev) => (prev + 1) % cards.length);
    setLastSwipe('Swiped Left → Next Card');
  };

  const prevCard = () => {
    setCurrentCard((prev) => (prev - 1 + cards.length) % cards.length);
    setLastSwipe('Swiped Right → Previous Card');
  };

  const handleSwipeUp = () => {
    setLastSwipe('Swiped Up');
  };

  const handleSwipeDown = () => {
    setLastSwipe('Swiped Down');
  };

  return (
    <div className="min-h-screen bg-gray-100 p-8">
      <div className="max-w-md mx-auto">
        <h1 className="text-3xl font-bold text-center mb-8 text-gray-800">
          Swipe Container Demo
        </h1>

        <div className="mb-6">
          <p className="text-center text-gray-600 mb-2">
            Try swiping on the card below:
          </p>
          <p className="text-center text-sm text-gray-500">
            Left/Right: Navigate cards | Up/Down: Custom actions
          </p>
        </div>

        <h3 className="text-2xl font-bold mb-4">
          {cards[currentCard].title}
        </h3>
        <SwipeContainer
          onSwipeLeft={nextCard}
          onSwipeRight={prevCard}
          onSwipeUp={handleSwipeUp}
          onSwipeDown={handleSwipeDown}
          threshold={50}
          className="mb-6"
        >
          {cards[currentCard].element ? cards[currentCard].element : <div className={`${cards[currentCard].color} rounded-lg shadow-lg p-8 text-center text-white transition-all duration-300 cursor-grab active:cursor-grabbing`}>
            <h2 className="text-2xl font-bold mb-4">
              {cards[currentCard].title}
            </h2>
            <p className="text-lg mb-4">
              Card {currentCard + 1} of {cards.length}
            </p>
            <p className="text-sm opacity-90">
              Swipe to navigate or perform actions
            </p>
          </div>
          }

        </SwipeContainer>

        <div className="text-center space-y-4">
          <div className="flex justify-center space-x-2">
            {cards.map((_, index) => (
              <div
                key={index}
                className={`w-2 h-2 rounded-full transition-colors ${index === currentCard ? 'bg-gray-600' : 'bg-gray-300'
                  }`}
              />
            ))}
          </div>

          {lastSwipe && (
            <div className="bg-white rounded-lg p-4 shadow-md">
              <span className="text-lg mb-4">
                Card {currentCard + 1} of {cards.length}
              </span>
              <p className="text-sm text-gray-600">Last Action:</p>
              <p className="font-semibold text-gray-800">{lastSwipe}</p>
            </div>
          )}

          <div className="flex justify-center space-x-4 mt-6">
            <button
              onClick={prevCard}
              className="px-4 py-2 bg-gray-600 text-white rounded-lg hover:bg-gray-700 transition-colors"
            >
              ← Previous
            </button>
            <button
              onClick={nextCard}
              className="px-4 py-2 bg-gray-600 text-white rounded-lg hover:bg-gray-700 transition-colors"
            >
              Next →
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};

export default SwipeDemo;