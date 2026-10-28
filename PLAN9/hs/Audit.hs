-- Audit.hs - the light-garden audit checked by OMI.WSN (boxedPINE types/, omi-types)
-- against what zlazy answered in Plan 9 (src/audit.rc), and the grammar normalized.
--   ghc -O -i<types>/src -o audit PLAN9/hs/Audit.hs
--   ./audit audit/audit.wsn audit/answers     (both from audit.rc's output disk)
import qualified OMI.WSN as WSN
import OMI.Kernel
import System.Environment (getArgs)

toByte :: Int -> Byte
toByte n = B (nib (n `div` 16)) (nib (n `mod` 16))
  where nib k = N (bit 8 k) (bit 4 k) (bit 2 k) (bit 1 k)
        bit w k = if (k `div` w) `mod` 2 == 1 then I else O

fromByte :: Byte -> Int
fromByte (B (N a b c d) (N e f g h)) = foldl (\acc x -> acc * 2 + v x) 0 [a, b, c, d, e, f, g, h]
  where v O = 0
        v I = 1

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
    WSN.NoRead -> fail "the Plan 9 grammar does not parse"
  let normal = map (toEnum . fromByte) (WSN.render g)
  putStrLn ("render (parse audit.wsn) == audit.wsn: " ++ show (normal == text))
  rows <- map words . lines <$> readFile answersFile
  let check (f : rec : err : rest) =
        let r = unwords rest
            h = (yn (WSN.accepts g (bytes "RECORD") (bytes r)), yn (WSN.accepts g (bytes "ERRORRECORD") (bytes r)))
        in if h == (rec, err) then Nothing else Just (f ++ " " ++ r ++ " Plan 9 " ++ rec ++ "/" ++ err ++ " Haskell " ++ fst h ++ "/" ++ snd h)
      check _ = Just "malformed row"
      differ = [d | Just d <- map check rows]
  putStrLn (show (length rows) ++ " records, " ++ show (length rows * 2) ++ " answers; differing from zlazy: " ++ show (length differ))
  mapM_ putStrLn (take 5 differ)
  -- must-rejects: what the contract refuses
  let accepted t = yn (WSN.accepts g (bytes "RECORD") (bytes t))
  putStrLn ("a key the audit has (color) accepted: " ++ accepted "{\"test\":\"x\",\"color\":1}")
  putStrLn ("an unknown key (bogus) accepted: " ++ accepted "{\"test\":\"x\",\"bogus\":1}")
  putStrLn ("white space accepted: " ++ accepted "{\"test\": \"x\"}")
  putStrLn ("a leading zero accepted: " ++ accepted "{\"count\":01}")
