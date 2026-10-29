export interface Resolution {
  format: string;
  width: number;
  height: number;
  index: number;
}

export interface Camera {
  id: string | number;
  resolutions: Resolution[];
}

export interface CameraActived {
  id: string | number;
  resolution: Resolution;
}
