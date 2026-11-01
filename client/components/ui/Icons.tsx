import PixelSprite from "./PixelSprite";
import { COIN, EMOTION_GLYPH, LANTERN_ICON, MOON, NOTICE, SEAL, SUN } from "../../game/pixelArt";
import type { Emotion } from "@grudgebound/shared";

type P = { scale?: number; title?: string; className?: string };

export const CoinIcon = ({ scale = 2, ...p }: P) => <PixelSprite map={COIN} scale={scale} {...p} />;
export const LanternIcon = ({ scale = 2, ...p }: P) => <PixelSprite map={LANTERN_ICON} scale={scale} {...p} />;
export const MoonIcon = ({ scale = 2, ...p }: P) => <PixelSprite map={MOON} scale={scale} {...p} />;
export const SunIcon = ({ scale = 2, ...p }: P) => <PixelSprite map={SUN} scale={scale} {...p} />;
export const NoticeIcon = ({ scale = 2, ...p }: P) => <PixelSprite map={NOTICE} scale={scale} {...p} />;
export const SealIcon = ({ scale = 2, ...p }: P) => <PixelSprite map={SEAL} scale={scale} {...p} />;
export const EmotionIcon = ({ emotion, scale = 2, ...p }: P & { emotion: Emotion }) => (
  <PixelSprite map={EMOTION_GLYPH[emotion]} scale={scale} {...p} />
);
