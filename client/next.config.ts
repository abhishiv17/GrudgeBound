import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  reactStrictMode: true,
  // The shared package is plain TypeScript source; let Next compile it.
  transpilePackages: ["@grudgebound/shared"],
};

export default nextConfig;
