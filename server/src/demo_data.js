const DEMO_USERS = [
  {
    username: 'vita_explorer',
    full_name: 'PS Vita Photography',
    bio: 'OLED screen appreciator | Homebrew & handheld shots | Tokyo / Osaka',
    avatar_seed: 0,
    is_verified: true,
    post_count: 148,
    follower_count: 12400,
    following_count: 310,
  },
  {
    username: 'retro_pixels',
    full_name: 'Kaito Nakamura',
    bio: 'Handheld collector. Shooting sunsets & neon streets.',
    avatar_seed: 1,
    is_verified: false,
    post_count: 64,
    follower_count: 3890,
    following_count: 420,
  },
  {
    username: 'cafe_morning',
    full_name: 'Sora Sato',
    bio: 'Pour-over coffee, rainy afternoons, and cozy setups.',
    avatar_seed: 2,
    is_verified: false,
    post_count: 92,
    follower_count: 5120,
    following_count: 280,
  },
  {
    username: 'alpine_trails',
    full_name: 'Elena Vance',
    bio: 'Mountain ridges, misty forests, and analog film.',
    avatar_seed: 3,
    is_verified: true,
    post_count: 215,
    follower_count: 48200,
    following_count: 195,
  },
  {
    username: 'synthwave_lab',
    full_name: 'Mina Takahashi',
    bio: 'Retro hardware, CRT glow, and 80s palettes.',
    avatar_seed: 4,
    is_verified: false,
    post_count: 53,
    follower_count: 8760,
    following_count: 512,
  },
];

const DEMO_POSTS = [
  {
    id: 'demo_1',
    username: 'vita_explorer',
    scene: 'vita',
    caption: 'Browsing Instagram straight from a PlayStation Vita OLED screen feels surreal. #psvita #homebrew',
    like_count: 842,
    comment_count: 47,
    timestamp_text: '25m ago',
    is_video: false,
  },
  {
    id: 'demo_2',
    username: 'retro_pixels',
    scene: 'sunset',
    caption: 'Golden hour over the bay before heading back to Akihabara.',
    like_count: 319,
    comment_count: 18,
    timestamp_text: '2h ago',
    is_video: false,
  },
  {
    id: 'demo_3',
    username: 'cafe_morning',
    scene: 'coffee',
    caption: 'Morning espresso & Persona 4 Golden on the desk.',
    like_count: 512,
    comment_count: 29,
    timestamp_text: '4h ago',
    is_video: false,
  },
  {
    id: 'demo_4',
    username: 'alpine_trails',
    scene: 'mountain',
    caption: 'First light hitting the ridge at 2,400m. Worth the 4 AM hike.',
    like_count: 2410,
    comment_count: 112,
    timestamp_text: '7h ago',
    is_video: false,
  },
  {
    id: 'demo_5',
    username: 'synthwave_lab',
    scene: 'retro',
    caption: 'Testing custom shaders and vibrant gradients on handheld hardware.',
    like_count: 678,
    comment_count: 34,
    timestamp_text: '12h ago',
    is_video: false,
  },
  {
    id: 'demo_6',
    username: 'vita_explorer',
    scene: 'tokyo',
    caption: 'Late night walk through Shinjuku after the rain.',
    like_count: 1094,
    comment_count: 61,
    timestamp_text: '1d ago',
    is_video: false,
  },
  {
    id: 'demo_7',
    username: 'alpine_trails',
    scene: 'forest',
    caption: 'Quiet morning mist drifting through the cedar valley.',
    like_count: 1850,
    comment_count: 88,
    timestamp_text: '2d ago',
    is_video: false,
  },
  {
    id: 'demo_8',
    username: 'retro_pixels',
    scene: 'ocean',
    caption: 'Pacific horizon at dusk. Color gradient straight out of a postcard.',
    like_count: 420,
    comment_count: 22,
    timestamp_text: '3d ago',
    is_video: false,
  },
];

function userByName(username) {
  return DEMO_USERS.find((u) => u.username.toLowerCase() === username.toLowerCase()) || null;
}

function formatPost(p) {
  const author = userByName(p.username) || DEMO_USERS[0];
  return {
    id: p.id,
    username: author.username,
    full_name: author.full_name,
    avatar_url: `/api/media?demo_avatar=${author.avatar_seed}`,
    image_url: `/api/media?demo_scene=${encodeURIComponent(p.scene)}&w=480`,
    caption: p.caption,
    like_count: p.like_count,
    comment_count: p.comment_count,
    timestamp_text: p.timestamp_text,
    is_video: Boolean(p.is_video),
  };
}

export function getDemoFeed(maxId = '', limit = 5) {
  const offset = maxId ? Math.max(0, Number.parseInt(maxId, 10) || 0) : 0;
  const safeLimit = Math.max(1, Math.min(20, Number(limit) || 5));
  const slice = DEMO_POSTS.slice(offset, offset + safeLimit).map(formatPost);
  const nextOffset = offset + slice.length;
  const hasMore = nextOffset < DEMO_POSTS.length;
  return {
    posts: slice,
    next_max_id: hasMore ? String(nextOffset) : '',
    has_more: hasMore,
  };
}

export function searchDemoUsers(query = '') {
  const q = query.trim().toLowerCase();
  const matched = q
    ? DEMO_USERS.filter(
        (u) =>
          u.username.toLowerCase().includes(q) ||
          u.full_name.toLowerCase().includes(q) ||
          u.bio.toLowerCase().includes(q)
      )
    : DEMO_USERS;

  return {
    users: matched.map((u) => ({
      username: u.username,
      full_name: u.full_name,
      avatar_url: `/api/media?demo_avatar=${u.avatar_seed}`,
      is_verified: u.is_verified,
      follower_count: u.follower_count,
    })),
  };
}

export function getDemoProfile(username) {
  const u = userByName(username);
  if (!u) return null;

  let posts = DEMO_POSTS.filter((p) => p.username === u.username).map(formatPost);
  // Ensure every demo profile shows at least 3 grid items for layout testing
  if (posts.length < 3) {
    const extra = DEMO_POSTS.filter((p) => p.username !== u.username)
      .slice(0, 3 - posts.length)
      .map(formatPost);
    posts = posts.concat(extra);
  }

  return {
    username: u.username,
    full_name: u.full_name,
    bio: u.bio,
    avatar_url: `/api/media?demo_avatar=${u.avatar_seed}`,
    is_verified: u.is_verified,
    is_private: false,
    post_count: u.post_count,
    follower_count: u.follower_count,
    following_count: u.following_count,
    posts,
  };
}

