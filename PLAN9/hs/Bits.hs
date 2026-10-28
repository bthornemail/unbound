-- Bits.hs - for each token, one bit per production: does OMI.WSN (boxedPINE
-- types/, omi-types) admit the token under that production of the grammar?
-- The rows a Plan 9 contract compares against (grex.rc's expected.wsn).
--   ghc -O -i<types>/src -o bits PLAN9/hs/Bits.hs
--   ./bits grammar.wsn productions tokens > bits
import qualified OMI.WSN as WSN
import OMI.Kernel
import System.Environment (getArgs)

toByte :: Int -> Byte
toByte n = B (nib (n `div` 16)) (nib (n `mod` 16))
  where nib k = N (bit 8 k) (bit 4 k) (bit 2 k) (bit 1 k)
        bit w k = if (k `div` w) `mod` 2 == 1 then I else O

bytes :: String -> [Byte]
bytes = map (toByte . fromEnum)

main :: IO ()
main = do
  [grammarFile, productionsFile, tokensFile] <- getArgs
  text <- readFile grammarFile
  g <- case WSN.parse (bytes text) of
    WSN.Read g -> pure g
    WSN.NoRead -> fail "the grammar does not parse"
  ps <- lines <$> readFile productionsFile
  ts <- lines <$> readFile tokensFile
  let bit p t = case WSN.accepts g (bytes p) (bytes t) of { WSN.Yes -> '1'; WSN.No -> '0' }
  mapM_ (\t -> putStrLn [ bit p t | p <- ps ]) ts
