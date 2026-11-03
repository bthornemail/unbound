-- Outline.hs - on the host: a page's outline by omi-types' OMI.View, the goldens of zviews (in/views/tree.mjs runs it).
--	ghc -O -i<omi-types>/src -outputdir <tmp> -o <outline> Outline.hs; outline page > outline
import qualified Data.ByteString as BS
import Data.Bits (testBit)
import Data.Word (Word8)
import System.Environment (getArgs)
import OMI.Kernel (Bit(..), Nibble(..), Byte(..))
import qualified OMI.View as V

bit :: Word8 -> Int -> Bit
bit w i = if testBit w i then I else O

toB :: Word8 -> Byte
toB w = B (N (bit w 7) (bit w 6) (bit w 5) (bit w 4)) (N (bit w 3) (bit w 2) (bit w 1) (bit w 0))

fromB :: Byte -> Word8
fromB (B (N a b c d) (N e f g h)) = foldl (\n x -> n * 2 + (case x of I -> 1; O -> 0)) 0 [a, b, c, d, e, f, g, h]

main :: IO ()
main = do
  [f] <- getArgs
  page <- BS.readFile f
  BS.putStr (BS.pack (map fromB (V.outline (V.tree (map toB (BS.unpack page))))))
