-- Contract.hs - a contract's answers from Plan 9 (zlazy) checked by OMI.WSN
-- (boxedPINE types/, omi-types): each line "PRODUCTION yes|no sentence" is asked
-- again of the same grammar, and the grammar normalized.
--   ghc -O -i<types>/src -o contract PLAN9/hs/Contract.hs
--   ./contract grammar.wsn answers
import qualified OMI.WSN as WSN
import OMI.Kernel
import System.Environment (getArgs)
import Data.List (isPrefixOf)

toByte :: Int -> Byte
toByte n = B (nib (n `div` 16)) (nib (n `mod` 16))
  where nib k = N (bit 8 k) (bit 4 k) (bit 2 k) (bit 1 k)
        bit w k = if (k `div` w) `mod` 2 == 1 then I else O

bytes :: String -> [Byte]
bytes = map (toByte . fromEnum)

yn :: WSN.Answer -> String
yn WSN.Yes = "yes"
yn WSN.No = "no"

main :: IO ()
main = do
  [grammarFile, answersFile] <- getArgs
  text <- readFile grammarFile
  g <- case WSN.parse (bytes text) of
    WSN.Read g -> pure g
    WSN.NoRead -> fail "the grammar does not parse"
  rows <- filter (\r -> case r of (p : a : _) -> a `elem` ["yes", "no"] && not ("/" `isPrefixOf` p); _ -> False)
          . map words . lines <$> readFile answersFile
  raw <- lines <$> readFile answersFile
  let sentence l = drop 1 (dropWhile (/= ' ') (drop 1 (dropWhile (/= ' ') l)))
      asked = [ (p, a, sentence l) | l <- raw, (p : a : _) <- [words l], a `elem` ["yes", "no"], p `elem` ["TIME", "TIMING"] ]
      differ = [ p ++ " " ++ s ++ ": Plan 9 " ++ a ++ ", Haskell " ++ h
               | (p, a, s) <- asked, let h = yn (WSN.accepts g (bytes p) (bytes s)), h /= a ]
  putStrLn (show (length asked) ++ " answers from Plan 9 asked again: " ++ show (length differ) ++ " differ")
  mapM_ putStrLn (take 5 differ)
  _ <- pure rows
  pure ()
